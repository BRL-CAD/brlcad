/*                         T R E E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file shapes/tree/tree.cpp
 *
 * Procedural volumetric tree generator.
 */

#include "common.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/str.h"
#include "raytrace.h"
#include "rt/db_attr.h"
#include "rt/geom.h"
#include "wdb.h"

#include "../../libbu/json.hpp"

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

struct vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct rgb_color {
    unsigned char r = 105;
    unsigned char g = 78;
    unsigned char b = 52;
};

enum class output_mode {
    mesh,
    csg,
    analysis
};

enum class preset_type {
    broadleaf,
    conifer,
    palm,
    shrub,
    dead
};

enum class leaf_mode {
    none,
    simple,
    cluster,
    canopy
};

enum class detail_level {
    low,
    medium,
    high
};

struct tree_options {
    std::string outfile = "tree.g";
    std::string name = "tree";
    std::string settings_file;
    std::string settings_json;
    std::string demo_file;
    preset_type preset = preset_type::broadleaf;
    output_mode mode = output_mode::mesh;
    leaf_mode leaves = leaf_mode::simple;
    detail_level detail = detail_level::medium;
    uint32_t seed = 1;
    double height = 12000.0;
    double trunk_radius = 420.0;
    double age = 1.0;
    int levels = 4;
    int branch_sides = 10;
    int leaf_sides = 4;
    int leaf_count = 700;
    int max_branch_level = -1;
    int csg_max_branch_level = 2;
    double length_decay = 0.62;
    double radius_decay = 0.55;
    double segments_per_meter = 3.0;
    double branch_angle_min = 35.0;
    double branch_angle_max = 55.0;
    double gravity_bias = 0.25;
    double upward_bias = 0.45;
    double twist_deg = 137.5;
    double leaf_length = 90.0;
    double leaf_width = 35.0;
    double cluster_radius = 180.0;
    double offset_x = 0.0;
    double offset_y = 0.0;
    double offset_z = 0.0;
    std::vector<int> branching = {3, 4, 3};
    rgb_color bark_rgb = {105, 78, 52};
    rgb_color leaf_rgb = {48, 118, 62};
    bool overwrite = false;
    bool append = false;
};

struct tree_segment {
    vec3 base;
    vec3 mid;
    vec3 tip;
    double base_radius = 0.0;
    double tip_radius = 0.0;
    int level = 0;
    bool terminal = true;
};

struct leaf_instance {
    vec3 center;
    vec3 normal;
    double roll = 0.0;
    double length = 0.0;
    double width = 0.0;
    double thickness = 0.0;
};

struct tree_model {
    std::vector<tree_segment> segments;
    std::vector<leaf_instance> leaves;
};

struct canopy_ellipsoid {
    vec3 center;
    vec3 radii;
};

struct shader_branch_path {
    vec3 base;
    vec3 mid;
    vec3 tip;
};

struct bot_mesh {
    std::vector<fastf_t> vertices;
    std::vector<int> faces;
};

struct rng_state {
    uint32_t state = 1;

    explicit rng_state(uint32_t seed) : state(seed ? seed : 1) {}

    uint32_t next_u32()
    {
	uint32_t x = state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	state = x ? x : 1;
	return state;
    }

    double unit()
    {
	return static_cast<double>(next_u32()) / static_cast<double>(std::numeric_limits<uint32_t>::max());
    }

    double range(double lo, double hi)
    {
	return lo + (hi - lo) * unit();
    }
};

static vec3 operator+(const vec3 &a, const vec3 &b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static vec3 operator-(const vec3 &a, const vec3 &b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static vec3 operator*(const vec3 &a, double s) { return {a.x * s, a.y * s, a.z * s}; }
static vec3 operator/(const vec3 &a, double s) { return {a.x / s, a.y / s, a.z / s}; }

static double
dot(const vec3 &a, const vec3 &b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static vec3
cross(const vec3 &a, const vec3 &b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

static double
mag(const vec3 &v)
{
    return std::sqrt(dot(v, v));
}

static vec3
normalize(const vec3 &v)
{
    const double m = mag(v);
    if (m <= 0.0)
	return {0.0, 0.0, 1.0};
    return v / m;
}

static bool
finite_vec(const vec3 &v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

static void
basis_from_dir(const vec3 &dir, vec3 &u, vec3 &v)
{
    const vec3 d = normalize(dir);
    vec3 ref = std::fabs(d.z) < 0.9 ? vec3{0.0, 0.0, 1.0} : vec3{1.0, 0.0, 0.0};
    u = normalize(cross(ref, d));
    v = normalize(cross(d, u));
}

static void
to_point(point_t p, const vec3 &v)
{
    VSET(p, v.x, v.y, v.z);
}

static void
to_vect(vect_t p, const vec3 &v)
{
    VSET(p, v.x, v.y, v.z);
}

static vec3
segment_point(const tree_segment &seg, double t)
{
    const double u = 1.0 - t;
    return seg.base * (u * u) + seg.mid * (2.0 * u * t) + seg.tip * (t * t);
}

static vec3
segment_tangent(const tree_segment &seg, double t)
{
    return normalize((seg.mid - seg.base) * (2.0 * (1.0 - t)) + (seg.tip - seg.mid) * (2.0 * t));
}

static double
segment_radius(const tree_segment &seg, double t)
{
    return seg.base_radius + (seg.tip_radius - seg.base_radius) * t;
}

static std::string
num_string(double val)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;
    bu_vls_printf(&str, "%.17g", val);
    std::string ret = bu_vls_cstr(&str);
    bu_vls_free(&str);
    return ret;
}

static std::string
preset_name(preset_type p)
{
    switch (p) {
	case preset_type::broadleaf: return "broadleaf";
	case preset_type::conifer: return "conifer";
	case preset_type::palm: return "palm";
	case preset_type::shrub: return "shrub";
	case preset_type::dead: return "dead";
    }
    return "broadleaf";
}

static std::string
mode_name(output_mode m)
{
    switch (m) {
	case output_mode::mesh: return "mesh";
	case output_mode::csg: return "csg";
	case output_mode::analysis: return "analysis";
    }
    return "mesh";
}

static std::string
leaf_mode_name(leaf_mode m)
{
    switch (m) {
	case leaf_mode::none: return "none";
	case leaf_mode::simple: return "simple";
	case leaf_mode::cluster: return "cluster";
	case leaf_mode::canopy: return "canopy";
    }
    return "simple";
}

static std::string
detail_name(detail_level d)
{
    switch (d) {
	case detail_level::low: return "low";
	case detail_level::medium: return "medium";
	case detail_level::high: return "high";
    }
    return "medium";
}

static preset_type
parse_preset(const char *value)
{
    if (!value)
	throw std::runtime_error("--preset requires a value");
    if (BU_STR_EQUAL(value, "broadleaf")) return preset_type::broadleaf;
    if (BU_STR_EQUAL(value, "conifer")) return preset_type::conifer;
    if (BU_STR_EQUAL(value, "palm")) return preset_type::palm;
    if (BU_STR_EQUAL(value, "shrub")) return preset_type::shrub;
    if (BU_STR_EQUAL(value, "dead")) return preset_type::dead;
    throw std::runtime_error("--preset must be broadleaf, conifer, palm, shrub, or dead");
}

static output_mode
parse_mode(const char *value)
{
    if (!value)
	throw std::runtime_error("--mode requires a value");
    if (BU_STR_EQUAL(value, "mesh")) return output_mode::mesh;
    if (BU_STR_EQUAL(value, "csg")) return output_mode::csg;
    if (BU_STR_EQUAL(value, "analysis")) return output_mode::analysis;
    throw std::runtime_error("--mode must be mesh, csg, or analysis");
}

static leaf_mode
parse_leaf_mode(const char *value)
{
    if (!value)
	throw std::runtime_error("--leaf-mode requires a value");
    if (BU_STR_EQUAL(value, "none")) return leaf_mode::none;
    if (BU_STR_EQUAL(value, "simple")) return leaf_mode::simple;
    if (BU_STR_EQUAL(value, "cluster")) return leaf_mode::cluster;
    if (BU_STR_EQUAL(value, "canopy")) return leaf_mode::canopy;
    throw std::runtime_error("--leaf-mode must be none, simple, cluster, or canopy");
}

static detail_level
parse_detail(const char *value)
{
    if (!value)
	throw std::runtime_error("--detail requires a value");
    if (BU_STR_EQUAL(value, "low")) return detail_level::low;
    if (BU_STR_EQUAL(value, "medium")) return detail_level::medium;
    if (BU_STR_EQUAL(value, "high")) return detail_level::high;
    throw std::runtime_error("--detail must be low, medium, or high");
}

static double
parse_double_arg(const char *name, const char *value)
{
    if (!value)
	throw std::runtime_error(std::string(name) + " requires a value");
    char *end = NULL;
    const double d = std::strtod(value, &end);
    if (!end || *end != '\0' || !std::isfinite(d))
	throw std::runtime_error(std::string(name) + " requires a finite numeric value");
    return d;
}

static uint32_t
parse_seed_arg(const char *name, const char *value)
{
    if (!value)
	throw std::runtime_error(std::string(name) + " requires a value");
    char *end = NULL;
    const unsigned long l = std::strtoul(value, &end, 10);
    if (!end || *end != '\0' || l > std::numeric_limits<uint32_t>::max())
	throw std::runtime_error(std::string(name) + " requires a 32-bit unsigned integer value");
    return static_cast<uint32_t>(l);
}

static const char *
next_value(int argc, char **argv, int &i, const char *name)
{
    if (i + 1 >= argc)
	throw std::runtime_error(std::string(name) + " requires a value");
    return argv[++i];
}

static bool
split_long_option(const char *arg, std::string &key, const char *&value)
{
    if (!arg || arg[0] != '-' || arg[1] != '-')
	return false;
    const char *eq = std::strchr(arg + 2, '=');
    if (eq) {
	key.assign(arg + 2, static_cast<size_t>(eq - (arg + 2)));
	value = eq + 1;
    } else {
	key = arg + 2;
	value = NULL;
    }
    return true;
}

static void
usage(const char *argv0)
{
    bu_log("Usage: %s [options]\n\n", argv0);
    bu_log("Options:\n");
    bu_log("  -o, --output file.g          Output database [tree.g]\n");
    bu_log("  -n, --name name              Object name prefix [tree]\n");
    bu_log("      --settings file.json     Advanced JSON settings file\n");
    bu_log("      --preset name            broadleaf, conifer, palm, shrub, or dead [broadleaf]\n");
    bu_log("      --seed uint              Deterministic generation seed [1]\n");
    bu_log("      --height length          Overall tree height [12000]\n");
    bu_log("      --trunk-radius length    Trunk base radius [420]\n");
    bu_log("      --age ratio              Relative age/maturity in [0, 1] [1]\n");
    bu_log("      --detail name            low, medium, or high [medium]\n");
    bu_log("      --mode name              Output mode: mesh, csg, or analysis [mesh]\n");
    bu_log("      --leaf-mode name         none, simple, cluster, or canopy [simple]\n");
    bu_log("      --demo-file file.g       Write a grid of sample trees\n");
    bu_log("      --x-offset distance      Model X offset [0]\n");
    bu_log("      --y-offset distance      Model Y offset [0]\n");
    bu_log("      --z-offset distance      Model Z offset [0]\n");
    bu_log("  -f, --force                  Overwrite output file\n");
    bu_log("  -j, --append                 Append objects to an existing output file\n");
    bu_log("  -h, --help                   Print this help\n");
}

static std::string
read_text_file(const std::string &path)
{
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in)
	throw std::runtime_error("unable to open settings file '" + path + "'");
    std::ostringstream ss;
    ss << in.rdbuf();
    if (!in.good() && !in.eof())
	throw std::runtime_error("unable to read settings file '" + path + "'");
    return ss.str();
}

static void
reject_unknown_json_keys(const nlohmann::json &j, const std::vector<std::string> &allowed, const std::string &ctx)
{
    for (auto it = j.begin(); it != j.end(); ++it) {
	if (std::find(allowed.begin(), allowed.end(), it.key()) == allowed.end())
	    throw std::runtime_error(ctx + " contains unsupported key '" + it.key() + "'");
    }
}

static double
json_double(const nlohmann::json &j, const char *key, double default_value)
{
    if (!j.contains(key))
	return default_value;
    if (!j.at(key).is_number())
	throw std::runtime_error(std::string("settings key '") + key + "' must be numeric");
    const double val = j.at(key).get<double>();
    if (!std::isfinite(val))
	throw std::runtime_error(std::string("settings key '") + key + "' must be finite");
    return val;
}

static int
json_int(const nlohmann::json &j, const char *key, int default_value)
{
    if (!j.contains(key))
	return default_value;
    if (!j.at(key).is_number_integer())
	throw std::runtime_error(std::string("settings key '") + key + "' must be an integer");
    return j.at(key).get<int>();
}

static uint32_t
json_seed(const nlohmann::json &j, const char *key, uint32_t default_value)
{
    if (!j.contains(key))
	return default_value;
    if (!j.at(key).is_number_unsigned() && !j.at(key).is_number_integer())
	throw std::runtime_error(std::string("settings key '") + key + "' must be an integer");
    const uint64_t val = j.at(key).get<uint64_t>();
    if (val > std::numeric_limits<uint32_t>::max())
	throw std::runtime_error(std::string("settings key '") + key + "' must fit in uint32");
    return static_cast<uint32_t>(val);
}

static std::string
json_string(const nlohmann::json &j, const char *key, const std::string &default_value)
{
    if (!j.contains(key))
	return default_value;
    if (!j.at(key).is_string())
	throw std::runtime_error(std::string("settings key '") + key + "' must be a string");
    return j.at(key).get<std::string>();
}

static rgb_color
json_rgb(const nlohmann::json &j, const char *key, const rgb_color &default_value)
{
    if (!j.contains(key))
	return default_value;
    if (!j.at(key).is_array() || j.at(key).size() != 3)
	throw std::runtime_error(std::string("settings key '") + key + "' must be a 3-element RGB array");
    rgb_color c;
    int vals[3];
    for (int i = 0; i < 3; ++i) {
	if (!j.at(key).at(i).is_number_integer())
	    throw std::runtime_error(std::string("settings key '") + key + "' RGB values must be integers");
	vals[i] = j.at(key).at(i).get<int>();
	if (vals[i] < 0 || vals[i] > 255)
	    throw std::runtime_error(std::string("settings key '") + key + "' RGB values must be in [0, 255]");
    }
    c.r = static_cast<unsigned char>(vals[0]);
    c.g = static_cast<unsigned char>(vals[1]);
    c.b = static_cast<unsigned char>(vals[2]);
    return c;
}

static void
apply_preset(tree_options &opts)
{
    const std::string outfile = opts.outfile;
    const std::string name = opts.name;
    const std::string settings_file = opts.settings_file;
    const std::string demo_file = opts.demo_file;
    const bool overwrite = opts.overwrite;
    const bool append = opts.append;
    const output_mode mode = opts.mode;

    switch (opts.preset) {
	case preset_type::broadleaf:
	    opts = tree_options();
	    opts.branching = {4, 4, 3};
	    opts.length_decay = 0.62;
	    opts.radius_decay = 0.55;
	    opts.segments_per_meter = 3.0;
	    opts.branch_angle_min = 32.0;
	    opts.branch_angle_max = 58.0;
	    opts.upward_bias = 0.42;
	    opts.gravity_bias = 0.22;
	    opts.leaf_count = 12000;
	    opts.leaf_length = 130.0;
	    opts.leaf_width = 68.0;
	    opts.cluster_radius = 360.0;
	    break;
	case preset_type::conifer:
	    opts = tree_options();
	    opts.preset = preset_type::conifer;
	    opts.branching = {9, 6, 3};
	    opts.length_decay = 0.36;
	    opts.radius_decay = 0.46;
	    opts.segments_per_meter = 2.0;
	    opts.branch_angle_min = 62.0;
	    opts.branch_angle_max = 82.0;
	    opts.upward_bias = 0.10;
	    opts.gravity_bias = 0.48;
	    opts.leaf_count = 30000;
	    opts.leaf_length = 78.0;
	    opts.leaf_width = 22.0;
	    opts.cluster_radius = 210.0;
	    opts.csg_max_branch_level = 1;
	    opts.bark_rgb = {100, 74, 55};
	    opts.leaf_rgb = {34, 82, 57};
	    break;
	case preset_type::palm:
	    opts = tree_options();
	    opts.preset = preset_type::palm;
	    opts.levels = 2;
	    opts.branching = {12};
	    opts.length_decay = 0.36;
	    opts.radius_decay = 0.30;
	    opts.segments_per_meter = 2.0;
	    opts.branch_angle_min = 72.0;
	    opts.branch_angle_max = 88.0;
	    opts.upward_bias = 0.12;
	    opts.gravity_bias = 0.20;
	    opts.leaf_count = 3600;
	    opts.leaf_length = 560.0;
	    opts.leaf_width = 62.0;
	    opts.cluster_radius = 120.0;
	    opts.bark_rgb = {128, 94, 58};
	    opts.leaf_rgb = {40, 126, 76};
	    break;
	case preset_type::shrub:
	    opts = tree_options();
	    opts.preset = preset_type::shrub;
	    opts.height = 2600.0;
	    opts.trunk_radius = 140.0;
	    opts.levels = 3;
	    opts.branching = {6, 5};
	    opts.length_decay = 0.58;
	    opts.radius_decay = 0.56;
	    opts.segments_per_meter = 4.0;
	    opts.branch_angle_min = 38.0;
	    opts.branch_angle_max = 75.0;
	    opts.upward_bias = 0.34;
	    opts.gravity_bias = 0.18;
	    opts.leaf_count = 6000;
	    opts.leaf_length = 85.0;
	    opts.leaf_width = 45.0;
	    opts.cluster_radius = 260.0;
	    break;
	case preset_type::dead:
	    opts = tree_options();
	    opts.preset = preset_type::dead;
	    opts.leaves = leaf_mode::none;
	    opts.branching = {4, 3, 2};
	    opts.length_decay = 0.66;
	    opts.radius_decay = 0.50;
	    opts.segments_per_meter = 2.0;
	    opts.branch_angle_min = 32.0;
	    opts.branch_angle_max = 60.0;
	    opts.leaf_count = 0;
	    opts.bark_rgb = {115, 105, 88};
	    break;
    }

    opts.outfile = outfile;
    opts.name = name;
    opts.settings_file = settings_file;
    opts.demo_file = demo_file;
    opts.overwrite = overwrite;
    opts.append = append;
    opts.mode = mode;
}

static void
apply_settings_json(tree_options &opts, const std::string &raw)
{
    nlohmann::json j;
    try {
	j = nlohmann::json::parse(raw);
    } catch (const std::exception &e) {
	throw std::runtime_error(std::string("unable to parse settings JSON: ") + e.what());
    }
    if (!j.is_object())
	throw std::runtime_error("settings JSON root must be an object");
    reject_unknown_json_keys(j, {"version", "preset", "seed", "scale", "skeleton", "canopy", "realization", "materials"}, "settings JSON");
    if (!j.contains("version") || !j.at("version").is_number_integer() || j.at("version").get<int>() != 1)
	throw std::runtime_error("settings JSON version must be 1");

    if (j.contains("preset")) {
	const std::string p = json_string(j, "preset", preset_name(opts.preset));
	opts.preset = parse_preset(p.c_str());
	apply_preset(opts);
    }
    opts.seed = json_seed(j, "seed", opts.seed);

    if (j.contains("scale")) {
	const nlohmann::json &s = j.at("scale");
	if (!s.is_object())
	    throw std::runtime_error("settings scale must be an object");
	reject_unknown_json_keys(s, {"height", "trunk_radius", "age"}, "settings scale");
	opts.height = json_double(s, "height", opts.height);
	opts.trunk_radius = json_double(s, "trunk_radius", opts.trunk_radius);
	opts.age = json_double(s, "age", opts.age);
    }
    if (j.contains("skeleton")) {
	const nlohmann::json &s = j.at("skeleton");
	if (!s.is_object())
	    throw std::runtime_error("settings skeleton must be an object");
	reject_unknown_json_keys(s, {"levels", "segments_per_meter", "branching", "branch_angle_deg", "length_decay", "radius_decay", "gravity_bias", "upward_bias", "twist_deg"}, "settings skeleton");
	opts.levels = json_int(s, "levels", opts.levels);
	opts.segments_per_meter = json_double(s, "segments_per_meter", opts.segments_per_meter);
	opts.length_decay = json_double(s, "length_decay", opts.length_decay);
	opts.radius_decay = json_double(s, "radius_decay", opts.radius_decay);
	opts.gravity_bias = json_double(s, "gravity_bias", opts.gravity_bias);
	opts.upward_bias = json_double(s, "upward_bias", opts.upward_bias);
	opts.twist_deg = json_double(s, "twist_deg", opts.twist_deg);
	if (s.contains("branching")) {
	    if (!s.at("branching").is_array())
		throw std::runtime_error("settings skeleton branching must be an array");
	    opts.branching.clear();
	    for (const auto &v : s.at("branching")) {
		if (!v.is_number_integer())
		    throw std::runtime_error("settings skeleton branching values must be integers");
		opts.branching.push_back(v.get<int>());
	    }
	}
	if (s.contains("branch_angle_deg")) {
	    if (!s.at("branch_angle_deg").is_array() || s.at("branch_angle_deg").size() != 2)
		throw std::runtime_error("settings skeleton branch_angle_deg must be a two-element array");
	    opts.branch_angle_min = s.at("branch_angle_deg").at(0).get<double>();
	    opts.branch_angle_max = s.at("branch_angle_deg").at(1).get<double>();
	}
    }
    if (j.contains("canopy")) {
	const nlohmann::json &c = j.at("canopy");
	if (!c.is_object())
	    throw std::runtime_error("settings canopy must be an object");
	reject_unknown_json_keys(c, {"leaf_mode", "leaf_count", "leaf_length", "leaf_width", "cluster_radius"}, "settings canopy");
	const std::string lm = json_string(c, "leaf_mode", leaf_mode_name(opts.leaves));
	opts.leaves = parse_leaf_mode(lm.c_str());
	opts.leaf_count = json_int(c, "leaf_count", opts.leaf_count);
	opts.leaf_length = json_double(c, "leaf_length", opts.leaf_length);
	opts.leaf_width = json_double(c, "leaf_width", opts.leaf_width);
	opts.cluster_radius = json_double(c, "cluster_radius", opts.cluster_radius);
    }
    if (j.contains("realization")) {
	const nlohmann::json &r = j.at("realization");
	if (!r.is_object())
	    throw std::runtime_error("settings realization must be an object");
	reject_unknown_json_keys(r, {"mode", "branch_sides", "leaf_sides", "max_branch_level", "csg_max_branch_level"}, "settings realization");
	const std::string m = json_string(r, "mode", mode_name(opts.mode));
	opts.mode = parse_mode(m.c_str());
	opts.branch_sides = json_int(r, "branch_sides", opts.branch_sides);
	opts.leaf_sides = json_int(r, "leaf_sides", opts.leaf_sides);
	opts.max_branch_level = json_int(r, "max_branch_level", opts.max_branch_level);
	opts.csg_max_branch_level = json_int(r, "csg_max_branch_level", opts.csg_max_branch_level);
    }
    if (j.contains("materials")) {
	const nlohmann::json &m = j.at("materials");
	if (!m.is_object())
	    throw std::runtime_error("settings materials must be an object");
	reject_unknown_json_keys(m, {"bark_rgb", "leaf_rgb"}, "settings materials");
	opts.bark_rgb = json_rgb(m, "bark_rgb", opts.bark_rgb);
	opts.leaf_rgb = json_rgb(m, "leaf_rgb", opts.leaf_rgb);
    }
}

static nlohmann::json
effective_settings_json(const tree_options &opts)
{
    nlohmann::json j;
    j["version"] = 1;
    j["preset"] = preset_name(opts.preset);
    j["seed"] = opts.seed;
    j["scale"] = {{"height", opts.height}, {"trunk_radius", opts.trunk_radius}, {"age", opts.age}};
    j["skeleton"] = {
	{"levels", opts.levels},
	{"segments_per_meter", opts.segments_per_meter},
	{"branching", opts.branching},
	{"branch_angle_deg", {opts.branch_angle_min, opts.branch_angle_max}},
	{"length_decay", opts.length_decay},
	{"radius_decay", opts.radius_decay},
	{"gravity_bias", opts.gravity_bias},
	{"upward_bias", opts.upward_bias},
	{"twist_deg", opts.twist_deg}
    };
    j["canopy"] = {
	{"leaf_mode", leaf_mode_name(opts.leaves)},
	{"leaf_count", opts.leaf_count},
	{"leaf_length", opts.leaf_length},
	{"leaf_width", opts.leaf_width},
	{"cluster_radius", opts.cluster_radius}
    };
    j["realization"] = {
	{"mode", mode_name(opts.mode)},
	{"branch_sides", opts.branch_sides},
	{"leaf_sides", opts.leaf_sides},
	{"max_branch_level", opts.max_branch_level},
	{"csg_max_branch_level", opts.csg_max_branch_level}
    };
    j["materials"] = {
	{"bark_rgb", {opts.bark_rgb.r, opts.bark_rgb.g, opts.bark_rgb.b}},
	{"leaf_rgb", {opts.leaf_rgb.r, opts.leaf_rgb.g, opts.leaf_rgb.b}}
    };
    return j;
}

static void
finalize_options(tree_options &opts)
{
    if (opts.outfile.empty())
	throw std::runtime_error("output file name must not be empty");
    if (opts.name.empty())
	throw std::runtime_error("object name must not be empty");
    if (opts.append && opts.overwrite)
	throw std::runtime_error("-j append and -f force are mutually exclusive");
    if (!opts.demo_file.empty() && opts.append)
	throw std::runtime_error("--demo-file cannot be combined with append mode");
    if (opts.height <= 0.0 || !std::isfinite(opts.height))
	throw std::runtime_error("height must be positive and finite");
    if (opts.trunk_radius <= 0.0 || !std::isfinite(opts.trunk_radius))
	throw std::runtime_error("trunk radius must be positive and finite");
    if (opts.age < 0.0 || opts.age > 1.0 || !std::isfinite(opts.age))
	throw std::runtime_error("age must be in [0, 1]");
    if (opts.levels < 1 || opts.levels > 6)
	throw std::runtime_error("levels must be in [1, 6]");
    if (opts.branching.empty())
	opts.branching = {3};
    for (int b : opts.branching) {
	if (b < 0 || b > 12)
	    throw std::runtime_error("branching values must be in [0, 12]");
    }
    if (opts.branch_sides < 5 || opts.branch_sides > 32)
	throw std::runtime_error("branch_sides must be in [5, 32]");
    if (opts.segments_per_meter <= 0.0 || opts.segments_per_meter > 50.0 || !std::isfinite(opts.segments_per_meter))
	throw std::runtime_error("segments_per_meter must be positive, finite, and no greater than 50");
    if (opts.leaf_sides < 4 || opts.leaf_sides > 12)
	throw std::runtime_error("leaf_sides must be in [4, 12]");
    if (opts.leaf_count < 0 || opts.leaf_count > 50000)
	throw std::runtime_error("leaf_count must be in [0, 50000]");
    if (opts.max_branch_level == -1)
	opts.max_branch_level = opts.levels;
    if (opts.max_branch_level < 0 || opts.max_branch_level > opts.levels)
	throw std::runtime_error("max_branch_level must be between 0 and levels");
    if (opts.csg_max_branch_level < 0 || opts.csg_max_branch_level > opts.levels)
	throw std::runtime_error("csg_max_branch_level must be between 0 and levels");
    if (opts.length_decay <= 0.0 || opts.length_decay >= 1.0)
	throw std::runtime_error("length_decay must be greater than 0 and less than 1");
    if (opts.radius_decay <= 0.0 || opts.radius_decay >= 1.0)
	throw std::runtime_error("radius_decay must be greater than 0 and less than 1");
    if (opts.branch_angle_min < 0.0 || opts.branch_angle_max > 120.0 || opts.branch_angle_min > opts.branch_angle_max)
	throw std::runtime_error("branch_angle_deg values must satisfy 0 <= min <= max <= 120");
    if (opts.leaves == leaf_mode::none)
	opts.leaf_count = 0;
    if (opts.leaf_length <= 0.0 || opts.leaf_width <= 0.0 || opts.cluster_radius <= 0.0)
	throw std::runtime_error("leaf dimensions and cluster radius must be positive");
    if (!std::isfinite(opts.offset_x) || !std::isfinite(opts.offset_y) || !std::isfinite(opts.offset_z))
	throw std::runtime_error("model offsets must be finite");
    opts.settings_json = effective_settings_json(opts).dump();
}

static tree_options
parse_args(int argc, char **argv)
{
    tree_options opts;
    bool preset_supplied = false;
    bool mode_supplied = false;
    bool leaf_mode_supplied = false;
    bool seed_supplied = false;
    bool height_supplied = false;
    bool trunk_radius_supplied = false;
    bool age_supplied = false;
    bool detail_supplied = false;
    preset_type cli_preset = opts.preset;
    output_mode cli_mode = opts.mode;
    leaf_mode cli_leaf_mode = opts.leaves;
    detail_level cli_detail = opts.detail;
    uint32_t cli_seed = opts.seed;
    double cli_height = opts.height;
    double cli_trunk_radius = opts.trunk_radius;
    double cli_age = opts.age;

    for (int i = 1; i < argc; ++i) {
	const char *arg = argv[i];
	std::string key;
	const char *value = NULL;
	if (split_long_option(arg, key, value)) {
	    if (key == "help") {
		usage(argv[0]);
		bu_exit(EXIT_SUCCESS, NULL);
	    } else if (key == "output") {
		opts.outfile = value ? value : next_value(argc, argv, i, arg);
	    } else if (key == "name") {
		opts.name = value ? value : next_value(argc, argv, i, arg);
	    } else if (key == "settings") {
		opts.settings_file = value ? value : next_value(argc, argv, i, arg);
	    } else if (key == "preset") {
		opts.preset = parse_preset(value ? value : next_value(argc, argv, i, arg));
		cli_preset = opts.preset;
		preset_supplied = true;
	    } else if (key == "mode") {
		opts.mode = parse_mode(value ? value : next_value(argc, argv, i, arg));
		cli_mode = opts.mode;
		mode_supplied = true;
	    } else if (key == "leaf-mode") {
		opts.leaves = parse_leaf_mode(value ? value : next_value(argc, argv, i, arg));
		cli_leaf_mode = opts.leaves;
		leaf_mode_supplied = true;
	    } else if (key == "detail") {
		opts.detail = parse_detail(value ? value : next_value(argc, argv, i, arg));
		cli_detail = opts.detail;
		detail_supplied = true;
	    } else if (key == "seed") {
		opts.seed = parse_seed_arg(arg, value ? value : next_value(argc, argv, i, arg));
		cli_seed = opts.seed;
		seed_supplied = true;
	    } else if (key == "height") {
		opts.height = parse_double_arg(arg, value ? value : next_value(argc, argv, i, arg));
		cli_height = opts.height;
		height_supplied = true;
	    } else if (key == "trunk-radius") {
		opts.trunk_radius = parse_double_arg(arg, value ? value : next_value(argc, argv, i, arg));
		cli_trunk_radius = opts.trunk_radius;
		trunk_radius_supplied = true;
	    } else if (key == "age") {
		opts.age = parse_double_arg(arg, value ? value : next_value(argc, argv, i, arg));
		cli_age = opts.age;
		age_supplied = true;
	    } else if (key == "demo-file") {
		opts.demo_file = value ? value : next_value(argc, argv, i, arg);
	    } else if (key == "x-offset") {
		opts.offset_x = parse_double_arg(arg, value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "y-offset") {
		opts.offset_y = parse_double_arg(arg, value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "z-offset") {
		opts.offset_z = parse_double_arg(arg, value ? value : next_value(argc, argv, i, arg));
	    } else if (key == "force") {
		if (value)
		    throw std::runtime_error("--force does not take a value");
		opts.overwrite = true;
	    } else if (key == "append") {
		if (value)
		    throw std::runtime_error("--append does not take a value");
		opts.append = true;
	    } else {
		throw std::runtime_error(std::string("unknown option: ") + arg);
	    }
	    continue;
	}
	if (BU_STR_EQUAL(arg, "-h")) {
	    usage(argv[0]);
	    bu_exit(EXIT_SUCCESS, NULL);
	} else if (BU_STR_EQUAL(arg, "-o")) {
	    opts.outfile = next_value(argc, argv, i, arg);
	} else if (BU_STR_EQUAL(arg, "-n")) {
	    opts.name = next_value(argc, argv, i, arg);
	} else if (BU_STR_EQUAL(arg, "-f")) {
	    opts.overwrite = true;
	} else if (BU_STR_EQUAL(arg, "-j")) {
	    opts.append = true;
	} else {
	    throw std::runtime_error(std::string("unknown option: ") + arg);
	}
    }

    if (preset_supplied)
	apply_preset(opts);
    if (!opts.settings_file.empty())
	apply_settings_json(opts, read_text_file(opts.settings_file));
    if (preset_supplied)
	opts.preset = cli_preset;
    if (mode_supplied)
	opts.mode = cli_mode;
    if (leaf_mode_supplied)
	opts.leaves = cli_leaf_mode;
    if (seed_supplied)
	opts.seed = cli_seed;
    if (height_supplied)
	opts.height = cli_height;
    if (trunk_radius_supplied)
	opts.trunk_radius = cli_trunk_radius;
    if (age_supplied)
	opts.age = cli_age;
    if (detail_supplied)
	opts.detail = cli_detail;
    if (detail_supplied) {
	if (opts.detail == detail_level::low) {
	    opts.branch_sides = 7;
	    opts.leaf_count = std::min(opts.leaf_count, opts.preset == preset_type::palm ? 900 : 2400);
	} else if (opts.detail == detail_level::high) {
	    opts.branch_sides = std::max(opts.branch_sides, 14);
	    opts.leaf_count = std::max(opts.leaf_count, opts.preset == preset_type::palm ? 3200 : 11000);
	}
    }

    finalize_options(opts);
    return opts;
}

static void
append_vertex(bot_mesh &mesh, const vec3 &p)
{
    if (!finite_vec(p))
	throw std::runtime_error("generated non-finite mesh vertex");
    mesh.vertices.push_back(static_cast<fastf_t>(p.x));
    mesh.vertices.push_back(static_cast<fastf_t>(p.y));
    mesh.vertices.push_back(static_cast<fastf_t>(p.z));
}

static void
add_face(bot_mesh &mesh, int a, int b, int c)
{
    if (a == b || b == c || a == c)
	return;
    mesh.faces.push_back(a);
    mesh.faces.push_back(b);
    mesh.faces.push_back(c);
}

static void
add_tapered_tube(bot_mesh &mesh, const tree_segment &seg, int sides, double segments_per_meter)
{
    const double hmag = mag(seg.tip - seg.base);
    if (hmag <= 0.0 || seg.base_radius <= 0.0 || seg.tip_radius <= 0.0)
	return;
    const int base_offset = static_cast<int>(mesh.vertices.size() / 3);

    const int axial_steps = std::max(1, std::min(64, static_cast<int>(std::ceil((hmag / 1000.0) * segments_per_meter))));
    for (int step = 0; step <= axial_steps; ++step) {
	const double t = static_cast<double>(step) / static_cast<double>(axial_steps);
	const vec3 center = segment_point(seg, t);
	const double radius = segment_radius(seg, t);
	vec3 u, v;
	basis_from_dir(segment_tangent(seg, t), u, v);
	for (int i = 0; i < sides; ++i) {
	    const double a = 2.0 * M_PI * static_cast<double>(i) / static_cast<double>(sides);
	    const vec3 radial = u * std::cos(a) + v * std::sin(a);
	    append_vertex(mesh, center + radial * radius);
	}
    }
    const int base_center = static_cast<int>(mesh.vertices.size() / 3);
    append_vertex(mesh, segment_point(seg, 0.0));
    const int top_center = static_cast<int>(mesh.vertices.size() / 3);
    append_vertex(mesh, segment_point(seg, 1.0));

    for (int step = 0; step < axial_steps; ++step) {
	const int ring0 = base_offset + step * sides;
	const int ring1 = ring0 + sides;
	for (int i = 0; i < sides; ++i) {
	    const int j = (i + 1) % sides;
	    add_face(mesh, ring0 + i, ring0 + j, ring1 + j);
	    add_face(mesh, ring0 + i, ring1 + j, ring1 + i);
	}
    }

    const int top_offset = base_offset + axial_steps * sides;
    for (int i = 0; i < sides; ++i) {
	const int j = (i + 1) % sides;
	add_face(mesh, base_center, base_offset + j, base_offset + i);
	add_face(mesh, top_center, top_offset + i, top_offset + j);
    }
}

static void
add_leaf_volume(bot_mesh &mesh, const leaf_instance &leaf, int sides)
{
    vec3 u, v;
    basis_from_dir(leaf.normal, u, v);
    const double roll = leaf.roll;
    const vec3 ru = normalize(u * std::cos(roll) + v * std::sin(roll));
    const vec3 rv = normalize(cross(normalize(leaf.normal), ru));
    const int top = static_cast<int>(mesh.vertices.size() / 3);
    append_vertex(mesh, leaf.center + normalize(leaf.normal) * (0.5 * leaf.thickness));
    const int bottom = static_cast<int>(mesh.vertices.size() / 3);
    append_vertex(mesh, leaf.center - normalize(leaf.normal) * (0.5 * leaf.thickness));
    const int rim = static_cast<int>(mesh.vertices.size() / 3);
    for (int i = 0; i < sides; ++i) {
	const double a = 2.0 * M_PI * static_cast<double>(i) / static_cast<double>(sides);
	const vec3 p = leaf.center + ru * (0.5 * leaf.length * std::cos(a)) + rv * (0.5 * leaf.width * std::sin(a));
	append_vertex(mesh, p);
    }
    for (int i = 0; i < sides; ++i) {
	const int j = (i + 1) % sides;
	add_face(mesh, top, rim + i, rim + j);
	add_face(mesh, bottom, rim + j, rim + i);
    }
}

static void
add_needle_volume(bot_mesh &mesh, const leaf_instance &leaf)
{
    const vec3 axis = normalize(leaf.normal);
    vec3 u, v;
    basis_from_dir(axis, u, v);
    const vec3 ru = normalize(u * std::cos(leaf.roll) + v * std::sin(leaf.roll));
    const vec3 rv = normalize(cross(axis, ru));
    const double half_len = 0.5 * leaf.length;
    const double half_w = 0.5 * leaf.width;
    const int base = static_cast<int>(mesh.vertices.size() / 3);

    append_vertex(mesh, leaf.center - axis * half_len);
    append_vertex(mesh, leaf.center + axis * half_len);
    append_vertex(mesh, leaf.center + ru * half_w);
    append_vertex(mesh, leaf.center - ru * half_w);
    append_vertex(mesh, leaf.center + rv * half_w * 0.65);
    append_vertex(mesh, leaf.center - rv * half_w * 0.65);

    add_face(mesh, base + 0, base + 2, base + 4);
    add_face(mesh, base + 0, base + 4, base + 3);
    add_face(mesh, base + 0, base + 3, base + 5);
    add_face(mesh, base + 0, base + 5, base + 2);
    add_face(mesh, base + 1, base + 4, base + 2);
    add_face(mesh, base + 1, base + 3, base + 4);
    add_face(mesh, base + 1, base + 5, base + 3);
    add_face(mesh, base + 1, base + 2, base + 5);
}

static void
add_frond_volume(bot_mesh &mesh, const leaf_instance &leaf)
{
    const vec3 axis = normalize(leaf.normal);
    vec3 u, v;
    basis_from_dir(axis, u, v);
    const vec3 width_axis = normalize(u * std::cos(leaf.roll) + v * std::sin(leaf.roll));
    const vec3 normal = normalize(cross(axis, width_axis));
    const double half_t = 0.5 * leaf.thickness;
    const double half_len = 0.5 * leaf.length;
    const double half_width = 0.5 * leaf.width;
    const int top = static_cast<int>(mesh.vertices.size() / 3);
    const vec3 stations[5] = {
	leaf.center - axis * half_len,
	leaf.center - axis * (half_len * 0.35),
	leaf.center,
	leaf.center + axis * (half_len * 0.42),
	leaf.center + axis * half_len
    };
    const double widths[5] = {0.10, 0.62, 1.00, 0.70, 0.08};

    for (int side = 0; side < 2; ++side) {
	const vec3 offset = normal * (side == 0 ? half_t : -half_t);
	for (int i = 0; i < 5; ++i) {
	    append_vertex(mesh, stations[i] + width_axis * (half_width * widths[i]) + offset);
	    append_vertex(mesh, stations[i] - width_axis * (half_width * widths[i]) + offset);
	}
    }

    for (int i = 0; i < 4; ++i) {
	const int a = top + i * 2;
	const int b = top + i * 2 + 1;
	const int c = top + (i + 1) * 2;
	const int d = top + (i + 1) * 2 + 1;
	add_face(mesh, a, b, d);
	add_face(mesh, a, d, c);
    }
    const int bottom = top + 10;
    for (int i = 0; i < 4; ++i) {
	const int a = bottom + i * 2;
	const int b = bottom + (i + 1) * 2;
	const int c = bottom + i * 2 + 1;
	const int d = bottom + (i + 1) * 2 + 1;
	add_face(mesh, a, c, d);
	add_face(mesh, a, d, b);
    }
    for (int i = 0; i < 4; ++i) {
	const int lt0 = top + i * 2;
	const int lt1 = top + (i + 1) * 2;
	const int lb0 = bottom + i * 2;
	const int lb1 = bottom + (i + 1) * 2;
	const int rt0 = top + i * 2 + 1;
	const int rt1 = top + (i + 1) * 2 + 1;
	const int rb0 = bottom + i * 2 + 1;
	const int rb1 = bottom + (i + 1) * 2 + 1;
	add_face(mesh, lt0, lb0, lb1);
	add_face(mesh, lt0, lb1, lt1);
	add_face(mesh, rt0, rt1, rb1);
	add_face(mesh, rt0, rb1, rb0);
    }
    add_face(mesh, top + 0, bottom + 0, bottom + 1);
    add_face(mesh, top + 0, bottom + 1, top + 1);
    add_face(mesh, top + 8, top + 9, bottom + 9);
    add_face(mesh, top + 8, bottom + 9, bottom + 8);
}

static tree_model
generate_model(const tree_options &opts)
{
    tree_model model;
    rng_state rng(opts.seed);
    std::vector<size_t> current;

    const double h = opts.height * std::max(0.15, opts.age);
    const double trunk_h = opts.preset == preset_type::palm ? h * 0.78 : (opts.preset == preset_type::conifer ? h * 0.94 : h * 0.58);
    tree_segment trunk;
    trunk.base = {opts.offset_x, opts.offset_y, opts.offset_z};
    trunk.tip = {opts.offset_x, opts.offset_y, opts.offset_z + trunk_h};
    trunk.mid = (trunk.base + trunk.tip) * 0.5;
    trunk.base_radius = opts.trunk_radius * std::max(0.25, opts.age);
    trunk.tip_radius = std::max(opts.trunk_radius * 0.36, opts.trunk_radius * opts.radius_decay);
    trunk.level = 0;
    model.segments.push_back(trunk);
    current.push_back(0);

    for (int level = 1; level < opts.levels && level <= opts.max_branch_level; ++level) {
	std::vector<size_t> next;
	const int branch_count = opts.branching[std::min(static_cast<size_t>(level - 1), opts.branching.size() - 1)];
	for (size_t parent_idx : current) {
	    tree_segment &parent = model.segments[parent_idx];
	    parent.terminal = false;
	    const vec3 pdir = normalize(parent.tip - parent.base);
	    vec3 pu, pv;
	    basis_from_dir(pdir, pu, pv);
	    if (opts.preset == preset_type::conifer && level == 1 && parent_idx == 0) {
		const int whorls = 8;
		const int per_whorl = std::max(4, std::min(7, branch_count));
		const double parent_len = mag(parent.tip - parent.base);
		for (int w = 0; w < whorls; ++w) {
		    const double tier = static_cast<double>(w) / static_cast<double>(whorls - 1);
		    const double frac = 0.26 + 0.62 * tier + rng.range(-0.018, 0.018);
		    const double zshape = 1.0 - tier;
		    const double len_base = parent_len * (0.40 * zshape + 0.075) * rng.range(0.88, 1.12);
		    const int ring_count = std::max(3, per_whorl - (tier > 0.78 ? 2 : (tier > 0.58 ? 1 : 0)));
		    const double ring_twist = (opts.twist_deg * static_cast<double>(w) + rng.range(-10.0, 10.0)) * M_PI / 180.0;
		    for (int b = 0; b < ring_count; ++b) {
			const vec3 base = parent.base + (parent.tip - parent.base) * std::max(0.20, std::min(0.93, frac));
			const double az = ring_twist + 2.0 * M_PI * static_cast<double>(b) / static_cast<double>(ring_count) + rng.range(-0.08, 0.08);
			const vec3 radial = normalize(pu * std::cos(az) + pv * std::sin(az));
			const double zdir = -0.36 + 0.48 * tier;
			const vec3 dir = normalize(radial * (0.98 - 0.18 * tier) + vec3{0.0, 0.0, zdir});
			tree_segment child;
			child.base = base;
			child.tip = base + dir * len_base;
			child.mid = base + dir * (len_base * 0.54) + vec3{0.0, 0.0, -len_base * (0.09 + 0.05 * zshape)};
			child.base_radius = std::max(parent.base_radius * opts.radius_decay * (0.84 - 0.22 * tier), opts.trunk_radius * 0.020);
			child.tip_radius = std::max(child.base_radius * opts.radius_decay * 0.70, opts.trunk_radius * 0.010);
			child.level = level;
			model.segments.push_back(child);
			next.push_back(model.segments.size() - 1);
		    }
		}
		continue;
	    }
	    for (int b = 0; b < branch_count; ++b) {
		const double frac = opts.preset == preset_type::palm ? rng.range(0.88, 1.0) : rng.range(0.32, 0.94);
		const vec3 base = parent.base + (parent.tip - parent.base) * frac;
		const double az = (opts.twist_deg * static_cast<double>(b + level) + rng.range(-18.0, 18.0)) * M_PI / 180.0;
		const double ang = rng.range(opts.branch_angle_min, opts.branch_angle_max) * M_PI / 180.0;
		vec3 radial = normalize(pu * std::cos(az) + pv * std::sin(az));
		vec3 dir = normalize(pdir * std::cos(ang) + radial * std::sin(ang));
		dir = normalize(dir + vec3{0.0, 0.0, opts.upward_bias - opts.gravity_bias * static_cast<double>(level) / std::max(1, opts.levels - 1)});
		if (opts.preset == preset_type::conifer && level == 1)
		    dir = normalize(radial * 0.92 + vec3{0.0, 0.0, -0.28});
		if (opts.preset == preset_type::palm)
		    dir = normalize(radial * 0.96 + vec3{0.0, 0.0, rng.range(-0.22, 0.12)});
		const double parent_len = mag(parent.tip - parent.base);
		const double len = parent_len * opts.length_decay * rng.range(0.72, 1.12);
		tree_segment child;
		child.base = base;
		child.tip = base + dir * len;
		child.mid = base + dir * (len * 0.56);
		if (opts.preset == preset_type::broadleaf)
		    child.mid = child.mid + vec3{0.0, 0.0, len * rng.range(0.035, 0.090)};
		else if (opts.preset == preset_type::conifer)
		    child.mid = child.mid + vec3{0.0, 0.0, -len * rng.range(0.030, 0.075)};
		else if (opts.preset == preset_type::palm)
		    child.mid = child.mid + vec3{0.0, 0.0, -len * rng.range(0.040, 0.095)};
		else if (opts.preset == preset_type::shrub)
		    child.mid = child.mid + vec3{0.0, 0.0, len * rng.range(0.020, 0.060)};
		child.base_radius = std::max(parent.base_radius * opts.radius_decay * rng.range(0.76, 1.04), opts.trunk_radius * 0.018);
		child.tip_radius = std::max(child.base_radius * opts.radius_decay, opts.trunk_radius * 0.012);
		child.level = level;
		model.segments.push_back(child);
		next.push_back(model.segments.size() - 1);
	    }
	}
	current.swap(next);
	if (current.empty())
	    break;
    }

    if (opts.leaves != leaf_mode::none && opts.leaf_count > 0) {
	std::vector<const tree_segment *> terminals;
	for (const tree_segment &seg : model.segments) {
	    bool use_seg = false;
	    switch (opts.preset) {
		case preset_type::conifer:
		    use_seg = seg.level > 0;
		    break;
		case preset_type::palm:
		    use_seg = seg.terminal && seg.level > 0;
		    break;
		case preset_type::shrub:
		    use_seg = seg.terminal || seg.level > 0;
		    break;
		case preset_type::broadleaf:
		case preset_type::dead:
		    use_seg = seg.terminal || seg.level >= opts.levels - 1;
		    break;
	    }
	    if (use_seg)
		terminals.push_back(&seg);
	}
	if (opts.preset == preset_type::conifer && opts.mode == output_mode::mesh && !model.segments.empty()) {
	    for (const tree_segment &seg : model.segments) {
		if (seg.level != 1)
		    continue;
		const double branch_z = ((seg.base.z + seg.tip.z) * 0.5 - opts.offset_z) / opts.height;
		const double lower = std::max(0.0, std::min(1.0, (0.64 - branch_z) / 0.42));
		const int repeats = 2 + static_cast<int>(std::floor(lower * 7.0));
		for (int j = 0; j < repeats; ++j)
		    terminals.push_back(&seg);
	    }
	    for (int i = 0; i < 48; ++i)
		terminals.push_back(&model.segments.front());
	}
	if (terminals.empty())
	    terminals.push_back(&model.segments.back());
	for (int i = 0; i < opts.leaf_count; ++i) {
	    const tree_segment *seg = terminals[static_cast<size_t>(rng.next_u32()) % terminals.size()];
	    const vec3 dir = normalize(seg->tip - seg->base);
	    vec3 u, v;
	    basis_from_dir(dir, u, v);
	    double along = rng.range(0.45, 1.0);
	    double spread = opts.leaves == leaf_mode::cluster ? opts.cluster_radius : opts.cluster_radius * 0.45;
	    double length_scale = rng.range(0.72, 1.18);
	    double width_scale = rng.range(0.72, 1.18);
	    vect_t normal_jitter;
	    VSET(normal_jitter, rng.range(-0.55, 0.55), rng.range(-0.55, 0.55), rng.range(0.15, 0.85));

	    switch (opts.preset) {
		case preset_type::conifer:
		    along = rng.range(0.12, 1.0);
		    spread = opts.cluster_radius * (opts.leaves == leaf_mode::cluster ? 0.52 : 0.32);
		    if (seg->level == 0) {
			along = rng.range(0.46, 1.0);
			const double crown_t = (along - 0.46) / 0.54;
			spread = opts.height * (0.26 * (1.0 - crown_t) + 0.045);
		    }
		    length_scale = rng.range(0.62, 1.05);
		    width_scale = rng.range(0.52, 0.92);
		    VSET(normal_jitter, rng.range(-0.32, 0.32), rng.range(-0.32, 0.32), rng.range(0.0, 0.42));
		    break;
		case preset_type::palm:
		    along = rng.range(0.12, 1.0);
		    spread = opts.cluster_radius * 0.22;
		    length_scale = rng.range(0.74, 1.16);
		    width_scale = rng.range(0.52, 0.88);
		    VSET(normal_jitter, rng.range(-0.20, 0.20), rng.range(-0.20, 0.20), rng.range(0.02, 0.28));
		    break;
		case preset_type::shrub:
		    along = rng.range(0.32, 1.0);
		    spread = opts.cluster_radius * (opts.leaves == leaf_mode::cluster ? 0.78 : 0.48);
		    break;
		case preset_type::broadleaf:
		case preset_type::dead:
		    break;
	    }
	    if (opts.mode == output_mode::mesh) {
		switch (opts.preset) {
		    case preset_type::broadleaf:
			length_scale *= 1.85;
			width_scale *= 2.10;
			spread *= 1.10;
			break;
		    case preset_type::conifer:
		    {
			const double branch_z = ((seg->base.z + seg->tip.z) * 0.5 - opts.offset_z) / opts.height;
			const double lower = std::max(0.0, std::min(1.0, (0.64 - branch_z) / 0.42));
			length_scale *= 4.45 + 0.75 * lower;
			width_scale *= 5.75 + 1.35 * lower;
			spread = std::max(spread * (1.55 + 0.38 * lower), opts.height * 0.022);
			break;
		    }
		    case preset_type::palm:
			length_scale *= 1.65;
			width_scale *= 3.60;
			break;
		    case preset_type::shrub:
			length_scale *= 1.25;
			width_scale *= 1.35;
			break;
		    case preset_type::dead:
			break;
		}
	    }
	    vec3 center;
	    vec3 leaf_dir = dir;
	    if (opts.preset == preset_type::conifer) {
		leaf_dir = segment_tangent(*seg, along);
		basis_from_dir(leaf_dir, u, v);
		const double az = rng.range(0.0, 2.0 * M_PI);
		const double r = std::sqrt(rng.unit()) * spread;
		center = segment_point(*seg, along) + u * (std::cos(az) * r) + v * (std::sin(az) * r);
	    } else {
		center = seg->base + (seg->tip - seg->base) * along + u * rng.range(-spread, spread) + v * rng.range(-spread, spread);
	    }
	    leaf_instance leaf;
	    leaf.center = center;
	    leaf.normal = normalize(leaf_dir + vec3{normal_jitter[X], normal_jitter[Y], normal_jitter[Z]});
	    leaf.roll = rng.range(0.0, 2.0 * M_PI);
	    leaf.length = opts.leaf_length * length_scale;
	    leaf.width = opts.leaf_width * width_scale;
	    leaf.thickness = std::max(leaf.width * 0.10, opts.trunk_radius * 0.006);
	    model.leaves.push_back(leaf);
	}
    }

    return model;
}

static void
validate_mesh(const bot_mesh &mesh, const char *name)
{
    if (mesh.vertices.empty() || mesh.faces.empty())
	throw std::runtime_error(std::string(name) + " mesh is empty");
    if (mesh.vertices.size() % 3 != 0 || mesh.faces.size() % 3 != 0)
	throw std::runtime_error(std::string(name) + " mesh arrays have invalid lengths");
    const size_t vc = mesh.vertices.size() / 3;
    for (fastf_t v : mesh.vertices) {
	if (!std::isfinite(static_cast<double>(v)))
	    throw std::runtime_error(std::string(name) + " mesh contains a non-finite vertex");
    }
    for (int f : mesh.faces) {
	if (f < 0 || static_cast<size_t>(f) >= vc)
	    throw std::runtime_error(std::string(name) + " mesh face references an invalid vertex");
    }
}

static struct rt_wdb *
open_output_database(const tree_options &opts)
{
    const bool exists = bu_file_exists(opts.outfile.c_str(), NULL);
    if (exists && !opts.append && !opts.overwrite)
	bu_exit(EXIT_FAILURE, "ERROR: %s already exists. Use -f to overwrite or -j to append.\n", opts.outfile.c_str());

    if (exists && opts.append) {
	struct db_i *dbip = db_open(opts.outfile.c_str(), DB_OPEN_READWRITE);
	if (!dbip)
	    bu_exit(EXIT_FAILURE, "ERROR: unable to open %s for append\n", opts.outfile.c_str());
	if (db_dirbuild(dbip) < 0) {
	    db_close(dbip);
	    bu_exit(EXIT_FAILURE, "ERROR: unable to read directory from %s\n", opts.outfile.c_str());
	}
	struct rt_wdb *fp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
	if (!fp) {
	    db_close(dbip);
	    bu_exit(EXIT_FAILURE, "ERROR: unable to attach writer to %s\n", opts.outfile.c_str());
	}
	return fp;
    }
    struct rt_wdb *fp = wdb_fopen(opts.outfile.c_str());
    if (!fp)
	bu_exit(EXIT_FAILURE, "ERROR: unable to create %s\n", opts.outfile.c_str());
    mk_id(fp, "Procedural Tree");
    return fp;
}

static void
check_output_name_available(struct rt_wdb *fp, const std::string &name)
{
    if (db_lookup(fp->dbip, name.c_str(), LOOKUP_QUIET) != RT_DIR_NULL)
	bu_exit(EXIT_FAILURE, "ERROR: object %s already exists in output database\n", name.c_str());
}

static void
set_attr(struct rt_wdb *fp, const std::string &obj_name, const char *key, const std::string &value)
{
    if (db5_update_attribute(obj_name.c_str(), key, value.c_str(), fp->dbip) != 0)
	bu_exit(EXIT_FAILURE, "ERROR: unable to set %s attribute on %s\n", key, obj_name.c_str());
}

static void
write_metadata(struct rt_wdb *fp, const std::vector<std::string> &objects, const tree_options &opts)
{
    for (const std::string &obj_name : objects) {
	set_attr(fp, obj_name, "tree::generator", "tree");
	set_attr(fp, obj_name, "tree::metadata_version", "1");
	set_attr(fp, obj_name, "tree::settings_version", "1");
	set_attr(fp, obj_name, "tree::preset", preset_name(opts.preset));
	set_attr(fp, obj_name, "tree::seed", std::to_string(opts.seed));
	set_attr(fp, obj_name, "tree::geometry_mode", mode_name(opts.mode));
	set_attr(fp, obj_name, "tree::leaf_mode", leaf_mode_name(opts.leaves));
	set_attr(fp, obj_name, "tree::detail", detail_name(opts.detail));
	set_attr(fp, obj_name, "tree::height", num_string(opts.height));
	set_attr(fp, obj_name, "tree::trunk_radius", num_string(opts.trunk_radius));
	set_attr(fp, obj_name, "tree::age", num_string(opts.age));
	set_attr(fp, obj_name, "tree::solid_volumetric", "1");
	if (!opts.settings_file.empty())
	    set_attr(fp, obj_name, "tree::settings_file", opts.settings_file);
	set_attr(fp, obj_name, "tree::settings_json", opts.settings_json);
    }
}

static std::string
leaf_shader_params(const tree_options &opts)
{
    struct bu_vls params = BU_VLS_INIT_ZERO;
    bu_vls_printf(&params,
	    "leaf rgb=%u/%u/%u; plastic sh=6 sp=.12 di=.88",
	    static_cast<unsigned>(opts.leaf_rgb.r),
	    static_cast<unsigned>(opts.leaf_rgb.g),
	    static_cast<unsigned>(opts.leaf_rgb.b));
    std::string ret = bu_vls_cstr(&params);
    bu_vls_free(&params);
    return ret;
}

static std::string
vec_string(const vec3 &v)
{
    return num_string(v.x) + "/" + num_string(v.y) + "/" + num_string(v.z);
}

static std::vector<const tree_segment *>
selected_terminal_segments(const tree_model &model, size_t max_count)
{
    std::vector<const tree_segment *> candidates;
    for (const tree_segment &seg : model.segments) {
	if (seg.terminal && seg.level > 0)
	    candidates.push_back(&seg);
    }
    if (candidates.empty()) {
	for (const tree_segment &seg : model.segments) {
	    if (seg.level > 0)
		candidates.push_back(&seg);
	}
    }
    if (candidates.empty() && !model.segments.empty())
	candidates.push_back(&model.segments.back());
    if (!model.segments.empty()) {
	const vec3 origin = model.segments.front().base;
	std::sort(candidates.begin(), candidates.end(), [origin](const tree_segment *a, const tree_segment *b) {
	    const double ar = std::sqrt((a->tip.x - origin.x) * (a->tip.x - origin.x) + (a->tip.y - origin.y) * (a->tip.y - origin.y));
	    const double br = std::sqrt((b->tip.x - origin.x) * (b->tip.x - origin.x) + (b->tip.y - origin.y) * (b->tip.y - origin.y));
	    const double ascore = a->tip.z + ar * 0.18 + static_cast<double>(a->level) * 18.0;
	    const double bscore = b->tip.z + br * 0.18 + static_cast<double>(b->level) * 18.0;
	    return ascore > bscore;
	});
    }
    if (candidates.size() <= max_count)
	return candidates;

    std::vector<const tree_segment *> selected;
    selected.reserve(max_count);
    for (size_t i = 0; i < max_count; ++i) {
	const size_t idx = std::min(candidates.size() - 1, (i * candidates.size()) / max_count);
	selected.push_back(candidates[idx]);
    }
    return selected;
}

static std::vector<shader_branch_path>
shader_branch_paths(const tree_options &opts, const tree_model &model, size_t max_count)
{
    std::vector<shader_branch_path> paths;
    std::vector<const tree_segment *> branches;
    size_t branch_max = max_count;

    if (opts.preset == preset_type::conifer) {
	if (!model.segments.empty() && max_count > 0) {
	    const tree_segment &trunk = model.segments.front();
	    shader_branch_path leader;
	    leader.base = segment_point(trunk, 0.76);
	    leader.mid = segment_point(trunk, 0.88);
	    leader.tip = segment_point(trunk, 1.0);
	    paths.push_back(leader);
	    branch_max--;
	}
	for (const tree_segment &seg : model.segments) {
	    if (seg.level == 1)
		branches.push_back(&seg);
	}
	if (branches.empty()) {
	    for (const tree_segment &seg : model.segments) {
		if (seg.level > 0)
		    branches.push_back(&seg);
	    }
	}
	if (branches.size() > branch_max) {
	    std::sort(branches.begin(), branches.end(), [](const tree_segment *a, const tree_segment *b) {
		const double za = (a->base.z + a->tip.z) * 0.5;
		const double zb = (b->base.z + b->tip.z) * 0.5;
		return za < zb;
	    });
	    std::vector<const tree_segment *> selected;
	    selected.reserve(branch_max);
	    for (size_t i = 0; i < branch_max; ++i) {
		const size_t idx = std::min(branches.size() - 1, (i * branches.size()) / branch_max);
		selected.push_back(branches[idx]);
	    }
	    branches.swap(selected);
	}
    } else {
	branches = selected_terminal_segments(model, branch_max);
    }

    paths.reserve(paths.size() + branches.size());
    for (const tree_segment *seg : branches) {
	shader_branch_path path;
	path.base = seg->base;
	path.mid = seg->mid;
	path.tip = seg->tip;
	paths.push_back(path);
    }
    return paths;
}

static std::string
tree_shader_params(const tree_options &opts, const tree_model &model)
{
    struct bu_vls params = BU_VLS_INIT_ZERO;
    double shader_leaf_length = opts.leaf_length;
    double shader_leaf_width = opts.leaf_width;
    double cell = std::max(opts.leaf_length * 0.90, opts.leaf_width * 1.90);
    int leaves_per_cell = 8;
    double density = 0.96;
    double spread = std::max(opts.height * 0.38, opts.trunk_radius * 5.0);
    double crown_base = 0.34;
    double branch_radius = std::max(opts.leaf_length * 3.0, opts.trunk_radius * 1.6);
    double branch_bias = 0.72;
    double edge_density = 0.44;
    double layer_strength = 0.62;
    int foliage_style = 0;

    switch (opts.preset) {
	case preset_type::conifer:
	    foliage_style = 1;
	    shader_leaf_length = opts.leaf_length * 1.62;
	    shader_leaf_width = opts.leaf_width * 1.36;
	    cell = std::max(shader_leaf_length * 0.58, shader_leaf_width * 2.20);
	    leaves_per_cell = 8;
	    density = 1.0;
	    spread = std::max(opts.height * 0.29, opts.trunk_radius * 4.5);
	    crown_base = 0.18;
	    branch_radius = std::max(opts.leaf_length * 3.4, std::max(opts.cluster_radius * 1.22, opts.trunk_radius * 0.82));
	    branch_bias = 0.92;
	    edge_density = 0.14;
	    layer_strength = 0.72;
	    break;
	case preset_type::palm:
	    foliage_style = 2;
	    shader_leaf_length = opts.leaf_length * 0.76;
	    shader_leaf_width = opts.leaf_width * 0.76;
	    cell = std::max(opts.leaf_width * 2.2, opts.leaf_length * 0.23);
	    leaves_per_cell = 5;
	    density = 0.88;
	    spread = std::max(opts.height * 0.42, opts.trunk_radius * 5.0);
	    crown_base = 0.67;
	    branch_radius = std::max(opts.leaf_length * 0.82, opts.trunk_radius * 1.55);
	    branch_bias = 0.90;
	    edge_density = 0.24;
	    layer_strength = 0.42;
	    break;
	case preset_type::shrub:
	    foliage_style = 3;
	    cell = std::max(opts.leaf_length * 0.85, opts.leaf_width * 1.75);
	    leaves_per_cell = 8;
	    density = 1.0;
	    spread = std::max(opts.height * 0.50, opts.trunk_radius * 5.0);
	    crown_base = 0.14;
	    branch_radius = std::max(opts.leaf_length * 3.4, opts.trunk_radius * 1.8);
	    branch_bias = 0.70;
	    edge_density = 0.46;
	    layer_strength = 0.58;
	    break;
	case preset_type::broadleaf:
	case preset_type::dead:
	    break;
    }
    const size_t shader_branch_limit = opts.preset == preset_type::conifer ? 48 : 24;
    const std::vector<shader_branch_path> branches = shader_branch_paths(opts, model, shader_branch_limit);
    bu_vls_printf(&params,
	    "tree rgb=%u/%u/%u seed=%u style=%d origin=%s/%s/%s height=%s spread=%s crown_base=%s branch_bias=%s branch_radius=%s edge_density=%s layer_strength=%s branches=%zu branch_data=1 cell=%s density=%s length=%s width=%s leaves=%d",
	    static_cast<unsigned>(opts.leaf_rgb.r),
	    static_cast<unsigned>(opts.leaf_rgb.g),
	    static_cast<unsigned>(opts.leaf_rgb.b),
	    static_cast<unsigned>(opts.seed),
	    foliage_style,
	    num_string(opts.offset_x).c_str(),
	    num_string(opts.offset_y).c_str(),
	    num_string(opts.offset_z).c_str(),
	    num_string(opts.height).c_str(),
	    num_string(spread).c_str(),
	    num_string(crown_base).c_str(),
	    num_string(branch_bias).c_str(),
	    num_string(branch_radius).c_str(),
	    num_string(edge_density).c_str(),
	    num_string(layer_strength).c_str(),
	    branches.size(),
	    num_string(cell).c_str(),
	    num_string(density).c_str(),
	    num_string(shader_leaf_length).c_str(),
	    num_string(shader_leaf_width).c_str(),
	    leaves_per_cell);
    for (size_t i = 0; i < branches.size(); ++i) {
	bu_vls_printf(&params,
		" b%zu=%s/%s/%s",
		i,
		vec_string(branches[i].base).c_str(),
		vec_string(branches[i].mid).c_str(),
		vec_string(branches[i].tip).c_str());
    }
    bu_vls_printf(&params, "; plastic sh=6 sp=.12 di=.88");
    std::string ret = bu_vls_cstr(&params);
    bu_vls_free(&params);
    return ret;
}

static void
write_bot_solid(struct rt_wdb *fp, const std::string &name, bot_mesh &mesh)
{
    validate_mesh(mesh, name.c_str());
    check_output_name_available(fp, name);
    if (mk_bot(fp, name.c_str(), RT_BOT_SOLID, RT_BOT_CCW, 0,
	    mesh.vertices.size() / 3, mesh.faces.size() / 3,
	    mesh.vertices.data(), mesh.faces.data(), NULL, NULL) != 0) {
	bu_exit(EXIT_FAILURE, "ERROR: unable to write solid BoT %s\n", name.c_str());
    }
}

static int
write_mesh_tree(const tree_options &opts, const tree_model &model)
{
    struct rt_wdb *fp = open_output_database(opts);
    std::vector<std::string> metadata_objects;
    const std::string wood_solid = opts.name + "_wood.bot";
    const std::string leaf_solid = opts.name + "_leaves.bot";
    const std::string wood_region = opts.name + "_wood.r";
    const std::string leaf_region = opts.name + "_leaves.r";
    const std::string group_name = opts.name;
    check_output_name_available(fp, wood_region);
    check_output_name_available(fp, group_name);

    bot_mesh wood_mesh;
    for (const tree_segment &seg : model.segments)
	add_tapered_tube(wood_mesh, seg, std::max(5, opts.branch_sides - seg.level), opts.segments_per_meter);
    write_bot_solid(fp, wood_solid, wood_mesh);
    metadata_objects.push_back(wood_solid);

    unsigned char bark_rgb[] = {opts.bark_rgb.r, opts.bark_rgb.g, opts.bark_rgb.b};
    mk_region1(fp, wood_region.c_str(), wood_solid.c_str(), "plastic", "", bark_rgb);
    metadata_objects.push_back(wood_region);

    bool wrote_leaves = false;
    if (!model.leaves.empty()) {
	check_output_name_available(fp, leaf_region);
	bot_mesh leaf_mesh;
	for (const leaf_instance &leaf : model.leaves) {
	    if (opts.preset == preset_type::conifer)
		add_needle_volume(leaf_mesh, leaf);
	    else if (opts.preset == preset_type::palm)
		add_frond_volume(leaf_mesh, leaf);
	    else
		add_leaf_volume(leaf_mesh, leaf, opts.leaf_sides);
	}
	write_bot_solid(fp, leaf_solid, leaf_mesh);
	metadata_objects.push_back(leaf_solid);
	unsigned char leaf_rgb[] = {opts.leaf_rgb.r, opts.leaf_rgb.g, opts.leaf_rgb.b};
	const std::string shader = leaf_shader_params(opts);
	mk_region1(fp, leaf_region.c_str(), leaf_solid.c_str(), "stack", shader.c_str(), leaf_rgb);
	metadata_objects.push_back(leaf_region);
	wrote_leaves = true;
    }

    struct wmember group_head = WMEMBER_INIT_ZERO;
    BU_LIST_INIT(&group_head.l);
    (void)mk_addmember(wood_region.c_str(), &group_head.l, NULL, WMOP_UNION);
    if (wrote_leaves)
	(void)mk_addmember(leaf_region.c_str(), &group_head.l, NULL, WMOP_UNION);
    if (mk_lcomb(fp, group_name.c_str(), &group_head, 0, NULL, NULL, NULL, 0) != 0) {
	db_close(fp->dbip);
	bu_exit(EXIT_FAILURE, "ERROR: unable to write tree group\n");
    }
    metadata_objects.push_back(group_name);
    write_metadata(fp, metadata_objects, opts);
    db_close(fp->dbip);

    bu_log("Wrote %s mesh tree with %zu solid branch volumes and %zu solid leaf volumes\n",
	    opts.outfile.c_str(), model.segments.size(), model.leaves.size());
    return EXIT_SUCCESS;
}

static void
write_tgc_segment(struct rt_wdb *fp, const std::string &name, const tree_segment &seg)
{
    const vec3 h = seg.tip - seg.base;
    vec3 u, v;
    basis_from_dir(h, u, v);
    point_t base;
    vect_t height;
    vect_t a;
    vect_t b;
    vect_t c;
    vect_t d;
    to_point(base, seg.base);
    to_vect(height, h);
    to_vect(a, u * seg.base_radius);
    to_vect(b, v * seg.base_radius);
    to_vect(c, u * seg.tip_radius);
    to_vect(d, v * seg.tip_radius);
    if (mk_tgc(fp, name.c_str(), base, height, a, b, c, d) != 0)
	bu_exit(EXIT_FAILURE, "ERROR: unable to write TGC %s\n", name.c_str());
}

static void
write_pipe_segment(struct rt_wdb *fp, const std::string &name, const tree_segment &seg)
{
    struct bu_list pipe_head;
    const double chord = mag(seg.tip - seg.base);
    const int samples = chord > 1600.0 ? 5 : 3;

    mk_pipe_init(&pipe_head);
    for (int i = 0; i < samples; ++i) {
	const double t = static_cast<double>(i) / static_cast<double>(samples - 1);
	point_t p;
	const double radius = segment_radius(seg, t);
	const double od = std::max(radius * 2.0, SMALL_FASTF);
	const double bend = std::max(radius * 1.35, chord * 0.012);
	to_point(p, segment_point(seg, t));
	mk_add_pipe_pnt(&pipe_head, p, od, 0.0, bend);
    }
    if (mk_pipe(fp, name.c_str(), &pipe_head) != 0) {
	mk_pipe_free(&pipe_head);
	write_tgc_segment(fp, name, seg);
	return;
    }
    mk_pipe_free(&pipe_head);
}

static canopy_ellipsoid
default_canopy_ellipsoid(const tree_options &opts)
{
    canopy_ellipsoid ell;
    ell.center = {opts.offset_x, opts.offset_y, opts.offset_z + opts.height * 0.68};
    ell.radii = {
	std::max(opts.height * (opts.preset == preset_type::conifer ? 0.18 : 0.26), opts.trunk_radius * 2.0),
	std::max(opts.height * (opts.preset == preset_type::conifer ? 0.18 : 0.26), opts.trunk_radius * 2.0),
	std::max(opts.height * (opts.preset == preset_type::conifer ? 0.30 : 0.20), opts.trunk_radius * 2.0)
    };
    return ell;
}

static size_t
canopy_blob_count(const tree_options &opts, const tree_model &model)
{
    if (model.leaves.empty())
	return 1;

    size_t target = 7;
    switch (opts.preset) {
	case preset_type::broadleaf:
	    target = 9;
	    break;
	case preset_type::conifer:
	    target = 7;
	    break;
	case preset_type::palm:
	    target = 5;
	    break;
	case preset_type::shrub:
	    target = 5;
	    break;
	case preset_type::dead:
	    target = 1;
	    break;
    }
    if (opts.leaves == leaf_mode::simple)
	target = std::min(target, static_cast<size_t>(5));
    if (opts.leaves == leaf_mode::cluster)
	target = std::min(target + static_cast<size_t>(2), static_cast<size_t>(11));

    const size_t leaf_limited = std::max(static_cast<size_t>(1), model.leaves.size() / 35);
    return std::max(static_cast<size_t>(1), std::min(target, std::max(leaf_limited, static_cast<size_t>(1))));
}

static std::vector<canopy_ellipsoid>
build_conifer_canopy_ellipsoids(const tree_options &opts, const tree_model &model)
{
    std::vector<const tree_segment *> branches;
    for (const tree_segment &seg : model.segments) {
	if (seg.level == 1)
	    branches.push_back(&seg);
    }
    if (branches.empty()) {
	for (const tree_segment &seg : model.segments) {
	    if (seg.level > 0)
		branches.push_back(&seg);
	}
    }
    if (branches.empty())
	return {default_canopy_ellipsoid(opts)};

    std::vector<canopy_ellipsoid> ellipsoids;
    ellipsoids.reserve(branches.size());
    for (const tree_segment *seg : branches) {
	const double branch_z = ((seg->base.z + seg->tip.z) * 0.5 - opts.offset_z) / opts.height;
	const double tier = std::max(0.0, std::min(1.0, (branch_z - 0.22) / 0.70));
	const double spray_xy = std::max(opts.leaf_length * (1.55 + 0.48 * (1.0 - tier)), opts.cluster_radius * (0.58 + 0.18 * (1.0 - tier)));
	const double spray_z = std::max(opts.leaf_width * (3.6 + 1.0 * (1.0 - tier)), opts.trunk_radius * (0.18 + 0.07 * (1.0 - tier)));
	vec3 bmin = {
	    std::numeric_limits<double>::max(),
	    std::numeric_limits<double>::max(),
	    std::numeric_limits<double>::max()
	};
	vec3 bmax = {
	    -std::numeric_limits<double>::max(),
	    -std::numeric_limits<double>::max(),
	    -std::numeric_limits<double>::max()
	};
	for (int j = 0; j < 6; ++j) {
	    const double t = 0.24 + 0.76 * static_cast<double>(j) / 5.0;
	    const vec3 p = segment_point(*seg, t);
	    bmin.x = std::min(bmin.x, p.x);
	    bmin.y = std::min(bmin.y, p.y);
	    bmin.z = std::min(bmin.z, p.z);
	    bmax.x = std::max(bmax.x, p.x);
	    bmax.y = std::max(bmax.y, p.y);
	    bmax.z = std::max(bmax.z, p.z);
	}

	canopy_ellipsoid ell;
	ell.center = (bmin + bmax) * 0.5;
	ell.radii = {
	    std::max((bmax.x - bmin.x) * 0.52 + spray_xy, spray_xy),
	    std::max((bmax.y - bmin.y) * 0.52 + spray_xy, spray_xy),
	    std::max((bmax.z - bmin.z) * 0.62 + spray_z, spray_z)
	};
	ellipsoids.push_back(ell);
    }

    return ellipsoids.empty() ? std::vector<canopy_ellipsoid>{default_canopy_ellipsoid(opts)} : ellipsoids;
}

static std::vector<canopy_ellipsoid>
build_palm_canopy_ellipsoids(const tree_options &opts, const tree_model &model)
{
    std::vector<canopy_ellipsoid> ellipsoids;
    const std::vector<const tree_segment *> terminals = selected_terminal_segments(model, 12);
    canopy_ellipsoid crown;
    crown.center = {opts.offset_x, opts.offset_y, opts.offset_z + opts.height * 0.78};
    crown.radii = {
	std::max(opts.trunk_radius * 1.9, opts.leaf_width * 3.0),
	std::max(opts.trunk_radius * 1.9, opts.leaf_width * 3.0),
	std::max(opts.trunk_radius * 1.1, opts.leaf_width * 2.3)
    };
    ellipsoids.push_back(crown);

    for (const tree_segment *seg : terminals) {
	const vec3 h = seg->tip - seg->base;
	const vec3 center = seg->base + h * 0.68 + vec3{0.0, 0.0, opts.leaf_width * 0.28};
	canopy_ellipsoid ell;
	ell.center = center;
	ell.radii = {
	    std::max(std::fabs(h.x) * 0.46 + opts.leaf_width * 2.4, opts.leaf_length * 0.34),
	    std::max(std::fabs(h.y) * 0.46 + opts.leaf_width * 2.4, opts.leaf_length * 0.34),
	    std::max(opts.leaf_width * 1.55, opts.trunk_radius * 0.55)
	};
	ellipsoids.push_back(ell);
    }
    return ellipsoids;
}

static std::vector<canopy_ellipsoid>
build_canopy_ellipsoids(const tree_options &opts, const tree_model &model)
{
    if (model.leaves.empty())
	return {default_canopy_ellipsoid(opts)};

    if (opts.preset == preset_type::conifer)
	return build_conifer_canopy_ellipsoids(opts, model);
    if (opts.preset == preset_type::palm)
	return build_palm_canopy_ellipsoids(opts, model);

    const size_t blob_count = std::max(canopy_blob_count(opts, model), opts.preset == preset_type::broadleaf ? static_cast<size_t>(14) : static_cast<size_t>(8));
    const std::vector<const tree_segment *> terminals = selected_terminal_segments(model, std::min(blob_count, static_cast<size_t>(24)));
    if (terminals.empty())
	return {default_canopy_ellipsoid(opts)};

    vec3 central_sum = {0.0, 0.0, 0.0};
    vec3 central_min = {
	std::numeric_limits<double>::max(),
	std::numeric_limits<double>::max(),
	std::numeric_limits<double>::max()
    };
    vec3 central_max = {
	-std::numeric_limits<double>::max(),
	-std::numeric_limits<double>::max(),
	-std::numeric_limits<double>::max()
    };
    for (const leaf_instance &leaf : model.leaves) {
	const double pad = 0.45 * std::max(leaf.length, leaf.width);
	central_sum = central_sum + leaf.center;
	central_min.x = std::min(central_min.x, leaf.center.x - pad);
	central_min.y = std::min(central_min.y, leaf.center.y - pad);
	central_min.z = std::min(central_min.z, leaf.center.z - pad);
	central_max.x = std::max(central_max.x, leaf.center.x + pad);
	central_max.y = std::max(central_max.y, leaf.center.y + pad);
	central_max.z = std::max(central_max.z, leaf.center.z + pad);
    }

    std::vector<vec3> centers(terminals.size(), {0.0, 0.0, 0.0});
    std::vector<vec3> bmins(blob_count, {
	    std::numeric_limits<double>::max(),
	    std::numeric_limits<double>::max(),
	    std::numeric_limits<double>::max()
	});
    std::vector<vec3> bmaxs(blob_count, {
	    -std::numeric_limits<double>::max(),
	    -std::numeric_limits<double>::max(),
	    -std::numeric_limits<double>::max()
	});
    std::vector<size_t> counts(blob_count, 0);

    for (const leaf_instance &leaf : model.leaves) {
	size_t group = 0;
	double best_dist = std::numeric_limits<double>::max();
	for (size_t i = 0; i < terminals.size(); ++i) {
	    const double d = mag(leaf.center - terminals[i]->tip);
	    if (d < best_dist) {
		best_dist = d;
		group = i;
	    }
	}

	centers[group] = centers[group] + leaf.center;
	counts[group]++;
	const double pad = 0.58 * std::max(leaf.length, leaf.width);
	bmins[group].x = std::min(bmins[group].x, leaf.center.x - pad);
	bmins[group].y = std::min(bmins[group].y, leaf.center.y - pad);
	bmins[group].z = std::min(bmins[group].z, leaf.center.z - pad);
	bmaxs[group].x = std::max(bmaxs[group].x, leaf.center.x + pad);
	bmaxs[group].y = std::max(bmaxs[group].y, leaf.center.y + pad);
	bmaxs[group].z = std::max(bmaxs[group].z, leaf.center.z + pad);
    }

    std::vector<canopy_ellipsoid> ellipsoids;
    ellipsoids.reserve(terminals.size() + 1);
    canopy_ellipsoid central;
    central.center = central_sum / static_cast<double>(model.leaves.size());
    central.radii = {
	std::max((central_max.x - central_min.x) * 0.30, std::max(opts.height * 0.13, opts.trunk_radius * 2.0)),
	std::max((central_max.y - central_min.y) * 0.30, std::max(opts.height * 0.13, opts.trunk_radius * 2.0)),
	std::max((central_max.z - central_min.z) * 0.32, std::max(opts.height * 0.10, opts.trunk_radius * 1.5))
    };
    ellipsoids.push_back(central);
    for (size_t i = 0; i < terminals.size(); ++i) {
	const tree_segment *seg = terminals[i];
	const double seg_len = mag(seg->tip - seg->base);
	if (counts[i] == 0) {
	    bmins[i] = seg->tip - vec3{opts.leaf_length, opts.leaf_length, opts.leaf_length * 0.55};
	    bmaxs[i] = seg->tip + vec3{opts.leaf_length, opts.leaf_length, opts.leaf_length * 0.55};
	    centers[i] = seg->tip;
	    counts[i] = 1;
	}
	if (counts[i] < 3 && seg_len <= SMALL_FASTF)
	    continue;
	const vec3 tip_bias = seg->tip * 0.35;
	const vec3 center = (centers[i] + tip_bias) / static_cast<double>(counts[i]) * (static_cast<double>(counts[i]) / (static_cast<double>(counts[i]) + 0.35));
	canopy_ellipsoid ell;
	ell.center = center;
	ell.radii = {
	    std::max((bmaxs[i].x - bmins[i].x) * 0.42, std::max(opts.leaf_length * 2.6, opts.trunk_radius * 1.55)),
	    std::max((bmaxs[i].y - bmins[i].y) * 0.42, std::max(opts.leaf_length * 2.6, opts.trunk_radius * 1.55)),
	    std::max((bmaxs[i].z - bmins[i].z) * 0.48, std::max(opts.leaf_length * 1.55, opts.trunk_radius * 1.2))
	};
	ellipsoids.push_back(ell);
    }
    if (ellipsoids.empty())
	ellipsoids.push_back(default_canopy_ellipsoid(opts));
    return ellipsoids;
}

static std::string
canopy_solid_name(const tree_options &opts, size_t index)
{
    if (index == 0)
	return opts.name + "_canopy.s";
    struct bu_vls name = BU_VLS_INIT_ZERO;
    bu_vls_printf(&name, "%s_canopy_%03zu.s", opts.name.c_str(), index);
    std::string ret = bu_vls_cstr(&name);
    bu_vls_free(&name);
    return ret;
}

static std::string
conifer_cap_solid_name(const tree_options &opts)
{
    return opts.name + "_canopy_top.s";
}

static void
write_canopy_ell(struct rt_wdb *fp, const std::string &name, const canopy_ellipsoid &ell)
{
    point_t cpt;
    vect_t a;
    vect_t b;
    vect_t c;
    to_point(cpt, ell.center);
    to_vect(a, {ell.radii.x, 0.0, 0.0});
    to_vect(b, {0.0, ell.radii.y, 0.0});
    to_vect(c, {0.0, 0.0, ell.radii.z});
    if (mk_ell(fp, name.c_str(), cpt, a, b, c) != 0)
	bu_exit(EXIT_FAILURE, "ERROR: unable to write canopy ellipsoid %s\n", name.c_str());
}

static void
write_conifer_canopy_cap(struct rt_wdb *fp, const std::string &name, const tree_options &opts)
{
    point_t base;
    vect_t dir;
    const double cap_base_z = opts.offset_z + opts.height * 0.795;
    const double cap_height = opts.height * 0.165;
    const double base_radius = std::max(opts.height * 0.085, opts.trunk_radius * 1.55);
    const double nose_radius = std::max(opts.leaf_width * 0.75, opts.trunk_radius * 0.055);

    VSET(base, opts.offset_x, opts.offset_y, cap_base_z);
    VSET(dir, 0.0, 0.0, 1.0);
    if (mk_cone(fp, name.c_str(), base, dir, cap_height, base_radius, nose_radius) != 0)
	bu_exit(EXIT_FAILURE, "ERROR: unable to write conifer canopy cap %s\n", name.c_str());
}

static int
write_csg_tree(const tree_options &opts, const tree_model &model)
{
    struct rt_wdb *fp = open_output_database(opts);
    std::vector<std::string> metadata_objects;
    const std::string wood_comb = opts.name + "_wood.c";
    const std::string wood_region = opts.name + "_wood.r";
    const std::string leaves_region = opts.name + "_leaves.r";
    const std::string group_name = opts.name;
    check_output_name_available(fp, wood_comb);
    check_output_name_available(fp, wood_region);
    check_output_name_available(fp, group_name);

    struct wmember wood_head = WMEMBER_INIT_ZERO;
    BU_LIST_INIT(&wood_head.l);
    size_t csg_segments = 0;
    for (size_t i = 0; i < model.segments.size(); ++i) {
	const tree_segment &seg = model.segments[i];
	if (seg.level > opts.csg_max_branch_level)
	    continue;
	const std::string prim_name = opts.name + "_branch_" + std::to_string(i) + ".s";
	check_output_name_available(fp, prim_name);
	write_pipe_segment(fp, prim_name, seg);
	(void)mk_addmember(prim_name.c_str(), &wood_head.l, NULL, WMOP_UNION);
	metadata_objects.push_back(prim_name);
	csg_segments++;
    }
    if (csg_segments == 0)
	bu_exit(EXIT_FAILURE, "ERROR: CSG mode produced no branch primitives\n");
    if (mk_lcomb(fp, wood_comb.c_str(), &wood_head, 0, NULL, NULL, NULL, 0) != 0) {
	db_close(fp->dbip);
	bu_exit(EXIT_FAILURE, "ERROR: unable to write wood CSG comb\n");
    }
    metadata_objects.push_back(wood_comb);

    struct wmember wood_region_head = WMEMBER_INIT_ZERO;
    BU_LIST_INIT(&wood_region_head.l);
    (void)mk_addmember(wood_comb.c_str(), &wood_region_head.l, NULL, WMOP_UNION);
    unsigned char bark_rgb[] = {opts.bark_rgb.r, opts.bark_rgb.g, opts.bark_rgb.b};
    if (mk_lcomb(fp, wood_region.c_str(), &wood_region_head, 1, "plastic", "", bark_rgb, 0) != 0) {
	db_close(fp->dbip);
	bu_exit(EXIT_FAILURE, "ERROR: unable to write wood CSG region\n");
    }
    metadata_objects.push_back(wood_region);

    bool wrote_canopy = false;
    if (opts.leaves != leaf_mode::none && opts.leaf_count > 0) {
	check_output_name_available(fp, leaves_region);
	struct wmember leaves_head = WMEMBER_INIT_ZERO;
	BU_LIST_INIT(&leaves_head.l);
	const std::vector<canopy_ellipsoid> canopy_ells = build_canopy_ellipsoids(opts, model);
	for (size_t i = 0; i < canopy_ells.size(); ++i) {
	    const std::string canopy_solid = canopy_solid_name(opts, i);
	    check_output_name_available(fp, canopy_solid);
	    write_canopy_ell(fp, canopy_solid, canopy_ells[i]);
	    metadata_objects.push_back(canopy_solid);
	    (void)mk_addmember(canopy_solid.c_str(), &leaves_head.l, NULL, WMOP_UNION);
	}
	if (opts.preset == preset_type::conifer) {
	    const std::string cap_solid = conifer_cap_solid_name(opts);
	    check_output_name_available(fp, cap_solid);
	    write_conifer_canopy_cap(fp, cap_solid, opts);
	    metadata_objects.push_back(cap_solid);
	    (void)mk_addmember(cap_solid.c_str(), &leaves_head.l, NULL, WMOP_UNION);
	}
	/* Keep the tree as separate bark and foliage regions for shader control,
	 * but remove the low-detail CSG branch volume from the canopy proxies so
	 * rt does not report wood/foliage region overlaps.  This localizes the
	 * boolean cost to the procedural foliage region. */
	(void)mk_addmember(wood_comb.c_str(), &leaves_head.l, NULL, WMOP_SUBTRACT);
	unsigned char leaf_rgb[] = {opts.leaf_rgb.r, opts.leaf_rgb.g, opts.leaf_rgb.b};
	const std::string shader = tree_shader_params(opts, model);
	if (mk_lcomb(fp, leaves_region.c_str(), &leaves_head, 1, "stack", shader.c_str(), leaf_rgb, 0) != 0) {
	    db_close(fp->dbip);
	    bu_exit(EXIT_FAILURE, "ERROR: unable to write canopy region\n");
	}
	metadata_objects.push_back(leaves_region);
	wrote_canopy = true;
    }

    struct wmember group_head = WMEMBER_INIT_ZERO;
    BU_LIST_INIT(&group_head.l);
    (void)mk_addmember(wood_region.c_str(), &group_head.l, NULL, WMOP_UNION);
    if (wrote_canopy)
	(void)mk_addmember(leaves_region.c_str(), &group_head.l, NULL, WMOP_UNION);
    if (mk_lcomb(fp, group_name.c_str(), &group_head, 0, NULL, NULL, NULL, 0) != 0) {
	db_close(fp->dbip);
	bu_exit(EXIT_FAILURE, "ERROR: unable to write tree group\n");
    }
    metadata_objects.push_back(group_name);
    write_metadata(fp, metadata_objects, opts);
    db_close(fp->dbip);

    bu_log("Wrote %s %s tree with %zu CSG branch primitives%s\n",
	    opts.outfile.c_str(), mode_name(opts.mode).c_str(), csg_segments, wrote_canopy ? " and solid canopy volumes" : "");
    return EXIT_SUCCESS;
}

static int
write_single_tree(const tree_options &opts)
{
    tree_model model = generate_model(opts);
    if (opts.mode == output_mode::mesh)
	return write_mesh_tree(opts, model);
    tree_options csg_opts = opts;
    if (opts.mode == output_mode::analysis)
	csg_opts.csg_max_branch_level = std::min(csg_opts.csg_max_branch_level, 1);
    return write_csg_tree(csg_opts, model);
}

struct demo_spec {
    const char *name = nullptr;
    preset_type preset = preset_type::broadleaf;
    output_mode mode = output_mode::mesh;
    detail_level detail = detail_level::medium;
    uint32_t seed = 1;
    double height = 8000.0;
    double radius = 300.0;
    leaf_mode leaves = leaf_mode::simple;
};

static tree_options
demo_options(const tree_options &demo_opts, const demo_spec &spec, int index)
{
    const int cols = 5;
    const double dx = 9000.0;
    const double dy = 9000.0;
    tree_options opts;
    opts.outfile = demo_opts.demo_file;
    opts.name = spec.name;
    opts.preset = spec.preset;
    opts.mode = spec.mode;
    opts.detail = spec.detail;
    apply_preset(opts);
    opts.outfile = demo_opts.demo_file;
    opts.name = spec.name;
    opts.mode = spec.mode;
    opts.detail = spec.detail;
    opts.seed = spec.seed;
    opts.height = spec.height;
    opts.trunk_radius = spec.radius;
    opts.leaves = spec.leaves;
    opts.offset_x = static_cast<double>(index % cols) * dx;
    opts.offset_y = static_cast<double>(index / cols) * dy;
    opts.overwrite = index == 0;
    opts.append = index != 0;
    finalize_options(opts);
    return opts;
}

static int
write_demo_file(const tree_options &demo_opts)
{
    const std::vector<demo_spec> specs = {
	{"mesh_00_broadleaf", preset_type::broadleaf, output_mode::mesh, detail_level::medium, 11, 7600, 300, leaf_mode::cluster},
	{"mesh_01_conifer", preset_type::conifer, output_mode::mesh, detail_level::medium, 12, 8800, 300, leaf_mode::cluster},
	{"mesh_02_palm", preset_type::palm, output_mode::mesh, detail_level::medium, 13, 8200, 260, leaf_mode::simple},
	{"mesh_03_shrub", preset_type::shrub, output_mode::mesh, detail_level::medium, 14, 2800, 140, leaf_mode::cluster},
	{"mesh_04_dead", preset_type::dead, output_mode::mesh, detail_level::medium, 15, 7600, 310, leaf_mode::none},
	{"csg_05_broadleaf", preset_type::broadleaf, output_mode::csg, detail_level::medium, 16, 7600, 300, leaf_mode::canopy},
	{"csg_06_conifer", preset_type::conifer, output_mode::csg, detail_level::medium, 17, 8400, 310, leaf_mode::canopy},
	{"csg_07_palm", preset_type::palm, output_mode::csg, detail_level::medium, 18, 8200, 260, leaf_mode::canopy},
	{"csg_08_shrub", preset_type::shrub, output_mode::csg, detail_level::medium, 19, 2800, 140, leaf_mode::canopy},
	{"csg_09_dead", preset_type::dead, output_mode::csg, detail_level::medium, 20, 7600, 310, leaf_mode::none}
    };

    for (size_t i = 0; i < specs.size(); ++i) {
	tree_options opts = demo_options(demo_opts, specs[i], static_cast<int>(i));
	bu_log("[%zu] %s %s\n", i, mode_name(opts.mode).c_str(), opts.name.c_str());
	const int ret = write_single_tree(opts);
	if (ret != EXIT_SUCCESS)
	    return ret;
    }

    tree_options all_opts;
    all_opts.outfile = demo_opts.demo_file;
    all_opts.append = true;
    struct rt_wdb *fp = open_output_database(all_opts);
    if (db_lookup(fp->dbip, "all", LOOKUP_QUIET) != RT_DIR_NULL) {
	db_close(fp->dbip);
	bu_exit(EXIT_FAILURE, "ERROR: object all already exists in output database\n");
    }
    struct wmember all_head = WMEMBER_INIT_ZERO;
    BU_LIST_INIT(&all_head.l);
    for (const demo_spec &spec : specs)
	(void)mk_addmember(spec.name, &all_head.l, NULL, WMOP_UNION);
    if (mk_lcomb(fp, "all", &all_head, 0, NULL, NULL, NULL, 0) != 0) {
	db_close(fp->dbip);
	bu_exit(EXIT_FAILURE, "ERROR: unable to write demo all comb\n");
    }
    set_attr(fp, "all", "tree::generator", "tree");
    set_attr(fp, "all", "tree::metadata_version", "1");
    set_attr(fp, "all", "tree::solid_volumetric", "1");
    set_attr(fp, "all", "tree::demo_tree_count", std::to_string(specs.size()));
    db_close(fp->dbip);

    bu_log("Wrote %s with %zu sample trees and all comb\n", demo_opts.demo_file.c_str(), specs.size());
    return EXIT_SUCCESS;
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    try {
	const tree_options opts = parse_args(argc, argv);
	if (!opts.demo_file.empty())
	    return write_demo_file(opts);
	return write_single_tree(opts);
    } catch (const std::exception &e) {
	bu_log("ERROR: %s\n", e.what());
	usage(argv[0]);
	return EXIT_FAILURE;
    }
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
