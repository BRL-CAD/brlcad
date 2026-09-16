/*                B U I L D I N G _ S P E C . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file building_spec.cpp
 *
 * Building presets, schema adapters, and validation.
 */

#include "common.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "building.h"
#include "building_schema.h"
#include "../../libbu/json.hpp"

namespace building {
namespace {

using json = nlohmann::json;

constexpr double EARTH_RADIUS_METERS = 6378137.0;
constexpr double PI = 3.14159265358979323846;
constexpr double HEIGHT_CONSISTENCY_EPSILON_METERS = 1.0e-6;
constexpr size_t MAX_FOOTPRINT_POINTS = 512;
constexpr int MAX_LEVELS = 200;

const std::set<std::string> residential_types = {
    "apartments", "annexe", "barracks", "bungalow", "cabin", "detached",
    "dormitory", "farm", "ger", "hotel", "house", "houseboat", "residential",
    "semidetached_house", "static_caravan", "stilt_house", "terrace", "tree_house", "trullo"
};

const std::set<std::string> commercial_types = {
    "commercial", "industrial", "kiosk", "office", "retail", "supermarket", "warehouse"
};

const std::set<std::string> religious_types = {
    "religious", "cathedral", "chapel", "church", "kingdom_hall", "monastery",
    "mosque", "presbytery", "shrine", "synagogue", "temple", "pagoda"
};

const std::set<std::string> civic_types = {
    "bakehouse", "bridge", "civic", "clock_tower", "college", "fire_station",
    "government", "gatehouse", "hospital", "kindergarten", "museum", "public",
    "school", "toilets", "train_station", "transportation", "university"
};

const std::set<std::string> agricultural_types = {
    "barn", "conservatory", "cowshed", "farm_auxiliary", "greenhouse", "slurry_tank",
    "stable", "sty", "livestock"
};

const std::set<std::string> sports_types = {
    "grandstand", "pavilion", "riding_hall", "sports_hall", "sports_centre", "stadium"
};

const std::set<std::string> storage_types = {
    "allotment_house", "boathouse", "hangar", "hut", "shed"
};

const std::set<std::string> vehicle_types = {"carport", "garage", "garages", "parking"};

const std::set<std::string> technical_types = {
    "digester", "service", "tech_cab", "transformer_tower", "water_tower", "storage_tank", "silo"
};

const std::set<std::string> roof_shapes = {
    "flat", "gabled", "gabled_height_moved", "skillion", "saltbox", "hipped",
    "half-hipped", "side_hipped", "side_half-hipped", "hipped-and-gabled", "mansard",
    "gambrel", "bellcast_gable", "pyramidal", "crosspitched", "sawtooth", "butterfly",
    "cone", "dome", "onion", "round", "many", "parabolic", "pitched", "half-dome",
    "equal_hipped", "equal_mansard", "apse_gabled", "round_gabled"
};

std::string
lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
	return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string
normalized_token(std::string value)
{
    value = lower(value);
    for (char &c : value) {
	if (std::isspace(static_cast<unsigned char>(c)) || c == '-')
	    c = '_';
    }
    const std::string suffix = "_roof";
    if (value.size() > suffix.size() && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0)
	value.erase(value.size() - suffix.size());
    return value;
}

std::string
read_text_file(const std::string &path)
{
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in)
	throw std::runtime_error("unable to open input file: " + path);
    std::ostringstream out;
    out << in.rdbuf();
    if (!in.good() && !in.eof())
	throw std::runtime_error("unable to read input file: " + path);
    return out.str();
}

double
unit_scale(const std::string &unit)
{
    const std::string u = lower(unit);
    if (u == "m" || u == "meter" || u == "metre" || u == "meters" || u == "metres")
	return 1.0;
    if (u == "mm" || u == "millimeter" || u == "millimetre")
	return 0.001;
    if (u == "cm" || u == "centimeter" || u == "centimetre")
	return 0.01;
    if (u == "ft" || u == "foot" || u == "feet")
	return 0.3048;
    if (u == "in" || u == "inch" || u == "inches")
	return 0.0254;
    throw std::runtime_error("unsupported length unit '" + unit + "'");
}

double
json_number(const json &object, const char *key, double fallback, double scale = 1.0)
{
    if (!object.contains(key))
	return fallback;
    const json &value = object.at(key);
    if (!value.is_number())
	throw std::runtime_error(std::string("'") + key + "' must be numeric");
    const double result = value.get<double>() * scale;
    if (!std::isfinite(result))
	throw std::runtime_error(std::string("'") + key + "' must be finite");
    return result;
}

int
json_integer(const json &object, const char *key, int fallback)
{
    if (!object.contains(key))
	return fallback;
    if (!object.at(key).is_number_integer())
	throw std::runtime_error(std::string("'") + key + "' must be an integer");
    return object.at(key).get<int>();
}

bool
json_boolean(const json &object, const char *key, bool fallback)
{
    if (!object.contains(key))
	return fallback;
    if (!object.at(key).is_boolean())
	throw std::runtime_error(std::string("'") + key + "' must be a boolean");
    return object.at(key).get<bool>();
}

std::string
json_string(const json &object, const char *key, const std::string &fallback)
{
    if (!object.contains(key))
	return fallback;
    if (!object.at(key).is_string())
	throw std::runtime_error(std::string("'") + key + "' must be a string");
    return object.at(key).get<std::string>();
}

void
reject_unknown_keys(const json &object, const std::set<std::string> &allowed, const std::string &context)
{
    if (!object.is_object())
	throw std::runtime_error(context + " must be an object");
    for (auto it = object.begin(); it != object.end(); ++it) {
	if (allowed.find(it.key()) == allowed.end())
	    throw std::runtime_error(context + " contains unknown property '" + it.key() + "'");
    }
}

std::string
value_string(const json &value)
{
    if (value.is_string())
	return value.get<std::string>();
    if (value.is_null())
	return "null";
    return value.dump();
}

std::map<std::string, std::string>
string_map(const json &object, const std::string &context)
{
    if (!object.is_object())
	throw std::runtime_error(context + " must be an object");
    std::map<std::string, std::string> result;
    for (auto it = object.begin(); it != object.end(); ++it)
	result[it.key()] = value_string(it.value());
    return result;
}

rgb_color
named_color(const std::string &raw, rgb_color fallback)
{
    const std::string value = lower(raw);
    const std::map<std::string, rgb_color> colors = {
	{"black", {25, 25, 25}}, {"white", {235, 235, 232}}, {"gray", {135, 135, 135}},
	{"grey", {135, 135, 135}}, {"red", {175, 55, 45}}, {"brown", {125, 82, 50}},
	{"yellow", {220, 195, 80}}, {"green", {80, 130, 85}}, {"blue", {65, 105, 165}},
	{"orange", {215, 125, 55}}, {"beige", {205, 190, 155}}, {"tan", {190, 155, 105}},
	{"silver", {175, 180, 180}}
    };
    const auto found = colors.find(value);
    if (found != colors.end())
	return found->second;
    std::string hex = value;
    if (!hex.empty() && hex.front() == '#')
	hex.erase(hex.begin());
    if (hex.size() == 6 && std::all_of(hex.begin(), hex.end(), [](unsigned char c) { return std::isxdigit(c); })) {
	return {
	    static_cast<unsigned char>(std::stoul(hex.substr(0, 2), nullptr, 16)),
	    static_cast<unsigned char>(std::stoul(hex.substr(2, 2), nullptr, 16)),
	    static_cast<unsigned char>(std::stoul(hex.substr(4, 2), nullptr, 16))
	};
    }
    return fallback;
}

rgb_color
json_color(const json &object, const char *key, rgb_color fallback)
{
    if (!object.contains(key))
	return fallback;
    const json &value = object.at(key);
    if (value.is_string())
	return named_color(value.get<std::string>(), fallback);
    if (!value.is_array() || value.size() != 3)
	throw std::runtime_error(std::string("'") + key + "' must be an RGB array or CSS color string");
    rgb_color result;
    unsigned char *channels[] = {&result.r, &result.g, &result.b};
    for (size_t i = 0; i < 3; ++i) {
	if (!value.at(i).is_number_integer())
	    throw std::runtime_error(std::string("'") + key + "' RGB channels must be integers");
	const int channel = value.at(i).get<int>();
	if (channel < 0 || channel > 255)
	    throw std::runtime_error(std::string("'") + key + "' RGB channels must be in [0, 255]");
	*channels[i] = static_cast<unsigned char>(channel);
    }
    return result;
}

double
parse_osm_length(const std::string &raw)
{
    size_t parsed = 0;
    double value = 0.0;
    try {
	value = std::stod(raw, &parsed);
    } catch (const std::exception &) {
	throw std::runtime_error("invalid OSM length '" + raw + "'");
    }
    std::string suffix = lower(raw.substr(parsed));
    suffix.erase(std::remove_if(suffix.begin(), suffix.end(), [](unsigned char c) { return std::isspace(c); }), suffix.end());
    if (suffix.empty() || suffix == "m")
	return value;
    if (suffix == "ft" || suffix == "foot" || suffix == "feet" || suffix == "'")
	return value * 0.3048;
    if (suffix == "cm")
	return value * 0.01;
    if (suffix == "mm")
	return value * 0.001;
    throw std::runtime_error("unsupported unit in OSM length '" + raw + "'");
}

int
parse_osm_integer(const std::string &raw, const std::string &key)
{
    size_t parsed = 0;
    int value = 0;
    try {
	value = std::stoi(raw, &parsed);
    } catch (const std::exception &) {
	throw std::runtime_error("invalid OSM integer for " + key + ": '" + raw + "'");
    }
    if (parsed != raw.size())
	throw std::runtime_error("invalid OSM integer for " + key + ": '" + raw + "'");
    return value;
}

std::vector<point2>
rectangle(double width, double depth)
{
    return {{0.0, 0.0}, {width, 0.0}, {width, depth}, {0.0, depth}};
}

std::vector<point2>
circle(double radius, size_t sides = 24)
{
    std::vector<point2> result;
    result.reserve(sides);
    for (size_t i = 0; i < sides; ++i) {
	const double angle = 2.0 * PI * static_cast<double>(i) / static_cast<double>(sides);
	result.push_back({radius * std::cos(angle) + radius, radius * std::sin(angle) + radius});
    }
    return result;
}

double
signed_area(const std::vector<point2> &points)
{
    double area = 0.0;
    for (size_t i = 0; i < points.size(); ++i) {
	const point2 &a = points[i];
	const point2 &b = points[(i + 1) % points.size()];
	area += a.x * b.y - b.x * a.y;
    }
    return area * 0.5;
}

void
normalize_footprint(std::vector<point2> &points)
{
    if (points.size() > 1) {
	const point2 &a = points.front();
	const point2 &b = points.back();
	if (std::hypot(a.x - b.x, a.y - b.y) < 1.0e-9)
	    points.pop_back();
    }
    if (signed_area(points) < 0.0)
	std::reverse(points.begin(), points.end());
}

std::string
profile_for_type(const std::string &raw_type)
{
    const std::string type = lower(raw_type);
    if (type == "apartments" || type == "dormitory" || type == "hotel" || type == "barracks")
	return "apartments";
    if (residential_types.count(type))
	return "house";
    if (type == "industrial" || type == "warehouse" || type == "hangar")
	return "warehouse";
    if (type == "office" || type == "commercial" || type == "retail" || type == "supermarket")
	return "office";
    if (religious_types.count(type))
	return "church";
    if (agricultural_types.count(type))
	return type == "greenhouse" || type == "conservatory" ? "greenhouse" : "barn";
    if (sports_types.count(type))
	return "sports_hall";
    if (type == "garage" || type == "garages")
	return "garage";
    if (type == "parking")
	return "parking";
    if (type == "carport" || type == "roof")
	return "carport";
    if (type == "silo" || type == "storage_tank" || type == "digester" || type == "slurry_tank" || type == "water_tower")
	return "silo";
    if (type == "tower" || type == "clock_tower" || type == "transformer_tower")
	return "tower";
    if (civic_types.count(type))
	return "civic";
    if (storage_types.count(type))
	return "shed";
    if (technical_types.count(type))
	return "service";
    return "house";
}

void
set_type_specific_defaults(building_spec &spec, const std::string &requested_type)
{
    const std::string type = normalized_token(requested_type);
    spec.type = requested_type.empty() ? spec.type : requested_type;
    auto set_storeys = [&spec](int count, double height) {
	spec.levels_above = count;
	spec.floor_heights.assign(static_cast<size_t>(count), height);
    };
    if (type == "bungalow") {
	set_storeys(1, 2.8);
	spec.footprint = rectangle(14.0, 10.0);
    } else if (type == "cabin" || type == "beach_hut" || type == "allotment_house") {
	set_storeys(1, 2.5);
	spec.footprint = rectangle(6.0, 5.0);
    } else if (type == "kiosk" || type == "toilets" || type == "tech_cab") {
	set_storeys(1, 2.7);
	spec.footprint = rectangle(5.0, 4.0);
    } else if (type == "retail") {
	set_storeys(2, 3.8);
	spec.footprint = rectangle(28.0, 18.0);
    } else if (type == "supermarket") {
	set_storeys(1, 5.5);
	spec.footprint = rectangle(40.0, 28.0);
    }
    if (type == "stilt_house" || type == "tree_house") {
	spec.min_level = 1;
	spec.min_height = 3.0;
	spec.structure.enabled = true;
	spec.structure.system = "post_and_beam";
    }
    if (type == "trullo") {
	set_storeys(1, 2.8);
	spec.footprint = circle(4.0);
	spec.roof.shape = "cone";
    }
    if (type == "quonset_hut") {
	set_storeys(1, 4.0);
	spec.footprint = rectangle(12.0, 20.0);
	spec.wall_material = "metal";
	spec.roof.shape = "round";
    }
    if (type == "pagoda")
	spec.roof.shape = "pyramidal";
    if (type == "mosque")
	spec.roof.shape = "dome";
    if (type == "cathedral") {
	set_storeys(1, 12.0);
	spec.footprint = rectangle(26.0, 44.0);
	spec.wall_thickness = 0.60;
    }
    if (type == "synagogue") {
	set_storeys(1, 7.5);
	spec.footprint = circle(10.0, 8);
    }
    if (type == "school") {
	set_storeys(2, 4.0);
	spec.footprint = rectangle(42.0, 18.0);
	spec.structure.grid_x = 7.0;
	spec.structure.grid_y = 4.5;
    }
    if (type == "hospital") {
	set_storeys(6, 4.0);
	spec.footprint = rectangle(44.0, 28.0);
	spec.structure.grid_x = 7.3;
	spec.structure.grid_y = 5.6;
    }
    if (type == "grandstand") {
	set_storeys(1, 4.0);
	spec.walls = false;
	spec.auto_openings = false;
	spec.roof.shape = "skillion";
	spec.roof.height = 3.0;
	spec.roof.thickness = 0.25;
	spec.roof.material = "metal";
    }
    if (type == "stadium") {
	set_storeys(1, 10.0);
	spec.footprint = circle(28.0, 48);
	spec.auto_openings = false;
	spec.roof.shape = "dome";
	spec.roof.height = 8.0;
	spec.roof.thickness = 0.40;
	spec.roof.overhang = 0.0;
	spec.structure.enabled = false;
	spec.structure.system = "shell";
    }
    if (type == "water_tower") {
	set_storeys(1, 18.0);
	spec.footprint = circle(5.0);
	spec.min_height = 10.0;
	spec.structure.enabled = true;
	spec.structure.system = "reinforced_concrete_frame";
	spec.roof.shape = "dome";
    }
    if (type == "digester" || type == "storage_tank" || type == "slurry_tank") {
	set_storeys(1, type == "digester" ? 8.0 : 12.0);
	spec.footprint = circle(7.0);
	spec.roof.shape = type == "digester" ? "dome" : "cone";
    }
    if (type == "windmill") {
	set_storeys(4, 3.0);
	spec.footprint = circle(5.0);
	spec.wall_material = "stone";
	spec.roof.shape = "cone";
    }
    if (type == "bunker") {
	set_storeys(1, 3.0);
	spec.wall_thickness = 0.50;
	spec.wall_material = "reinforced_concrete";
	spec.foundation_thickness = 0.60;
	spec.roof.shape = "flat";
	spec.roof.height = 0.60;
	spec.roof.thickness = 0.60;
	spec.roof.overhang = 0.0;
	spec.roof.material = "reinforced_concrete";
    }
    if (type == "museum") {
	spec.roof.height = 0.35;
	spec.roof.thickness = 0.35;
    }
    if (type == "container") {
	constexpr double container_length = 12.192;
	constexpr double container_width = 2.438;
	constexpr double container_height = 2.896;
	set_storeys(1, container_height);
	spec.footprint = rectangle(container_length, container_width);
	spec.total_height = container_height;
	spec.roof.height = 0.10;
	spec.wall_thickness = 0.05;
	spec.auto_openings = false;
	spec.floor_heights.assign(1, spec.total_height - effective_roof_height(spec));
    }
}

building_spec
preset_impl(const std::string &raw_preset)
{
    const std::string preset = normalized_token(raw_preset);
    building_spec spec;
    spec.id = preset;
    spec.type = preset;
    spec.footprint = rectangle(12.0, 9.0);
    spec.wall_material = "brick";
    spec.wall_color = {190, 165, 135};
    spec.roof.shape = "gabled";
    spec.roof.height = 2.6;

    if (preset == "house") {
	spec.type = "house";
	spec.levels_above = 2;
	spec.floor_heights = {2.8, 2.8};
	spec.structure.system = "wood_frame";
	spec.wall_thickness = 0.18;
	spec.roof.material = "roof_tiles";
    } else if (preset == "apartments") {
	spec.type = "apartments";
	spec.footprint = rectangle(24.0, 16.0);
	spec.levels_above = 6;
	spec.floor_heights.assign(6, 3.0);
	spec.wall_material = "brick";
	spec.wall_color = {185, 150, 120};
	spec.roof.shape = "flat";
	spec.roof.height = 0.35;
	spec.structure = {"reinforced_concrete_frame", true, 6.0, 5.3, "rectangular", 0.40, 0.40, 0.45, 0.30, 0.55, "reinforced_concrete", {145, 145, 140}};
    } else if (preset == "office") {
	spec.type = "office";
	spec.footprint = rectangle(30.0, 20.0);
	spec.levels_above = 5;
	spec.floor_heights.assign(5, 3.6);
	spec.wall_material = "glass";
	spec.wall_color = {105, 145, 165};
	spec.roof.shape = "flat";
	spec.roof.height = 0.45;
	spec.structure = {"steel_frame", true, 7.5, 5.0, "rectangular", 0.35, 0.35, 0.40, 0.30, 0.60, "steel", {105, 110, 115}};
    } else if (preset == "warehouse") {
	spec.type = "warehouse";
	spec.footprint = rectangle(42.0, 26.0);
	spec.levels_above = 1;
	spec.floor_heights = {8.0};
	spec.wall_material = "metal";
	spec.wall_color = {165, 170, 170};
	spec.roof.shape = "gabled";
	spec.roof.height = 4.0;
	spec.roof.material = "metal";
	spec.structure = {"steel_portal_frame", true, 7.0, 6.5, "rectangular", 0.30, 0.30, 0.35, 0.25, 0.65, "steel", {100, 105, 110}};
    } else if (preset == "church") {
	spec.type = "church";
	spec.footprint = rectangle(20.0, 36.0);
	spec.levels_above = 1;
	spec.floor_heights = {9.0};
	spec.wall_material = "stone";
	spec.wall_color = {165, 155, 140};
	spec.wall_thickness = 0.45;
	spec.roof.shape = "gabled";
	spec.roof.height = 6.0;
	spec.roof.overhang = 0.35;
    } else if (preset == "barn") {
	spec.type = "barn";
	spec.footprint = rectangle(20.0, 30.0);
	spec.levels_above = 1;
	spec.floor_heights = {5.5};
	spec.wall_material = "wood";
	spec.wall_color = {145, 65, 50};
	spec.roof.shape = "gambrel";
	spec.roof.height = 5.0;
	spec.structure = {"timber_frame", true, 5.0, 5.0, "rectangular", 0.30, 0.30, 0.35, 0.25, 0.45, "wood", {115, 78, 48}};
    } else if (preset == "garage") {
	spec.type = "garage";
	spec.footprint = rectangle(8.0, 7.0);
	spec.levels_above = 1;
	spec.floor_heights = {3.0};
	spec.wall_material = "cement_block";
	spec.wall_color = {175, 175, 165};
	spec.roof.shape = "gabled";
	spec.roof.height = 1.5;
    } else if (preset == "parking") {
	spec.type = "parking";
	spec.footprint = rectangle(36.0, 24.0);
	spec.levels_above = 4;
	spec.floor_heights.assign(4, 3.2);
	spec.walls = false;
	spec.auto_openings = false;
	spec.roof.shape = "flat";
	spec.roof.height = 0.30;
	spec.roof.thickness = 0.30;
	spec.structure = {"reinforced_concrete_frame", true, 7.2, 6.0, "rectangular", 0.50, 0.50, 0.55, 0.40, 0.70, "reinforced_concrete", {145, 145, 140}};
    } else if (preset == "greenhouse") {
	spec.type = "greenhouse";
	spec.footprint = rectangle(18.0, 30.0);
	spec.levels_above = 1;
	spec.floor_heights = {3.0};
	spec.wall_thickness = 0.05;
	spec.wall_material = "glass";
	spec.wall_color = {150, 195, 185};
	spec.roof.shape = "round";
	spec.roof.height = 4.0;
	spec.roof.thickness = 0.04;
	spec.roof.material = "glass";
	spec.auto_openings = false;
	spec.structure = {"steel_frame", true, 3.0, 5.0, "rectangular", 0.10, 0.10, 0.10, 0.10, 0.16, "steel", {105, 115, 110}};
    } else if (preset == "sports_hall") {
	spec.type = "sports_hall";
	spec.footprint = rectangle(36.0, 24.0);
	spec.levels_above = 1;
	spec.floor_heights = {9.0};
	spec.wall_material = "metal";
	spec.wall_color = {165, 175, 185};
	spec.roof.shape = "round";
	spec.roof.height = 5.0;
	spec.structure = {"steel_long_span", true, 6.0, 8.0, "rectangular", 0.30, 0.30, 0.35, 0.30, 0.90, "steel", {100, 105, 110}};
    } else if (preset == "silo") {
	spec.type = "silo";
	spec.footprint = circle(6.0);
	spec.levels_above = 1;
	spec.floor_heights = {18.0};
	spec.wall_thickness = 0.25;
	spec.wall_material = "steel";
	spec.wall_color = {175, 180, 180};
	spec.auto_openings = false;
	spec.roof.shape = "cone";
	spec.roof.height = 4.0;
	spec.roof.material = "steel";
	spec.roof.overhang = 0.0;
    } else if (preset == "tower") {
	spec.type = "tower";
	spec.footprint = rectangle(12.0, 12.0);
	spec.levels_above = 10;
	spec.floor_heights.assign(10, 3.4);
	spec.wall_material = "stone";
	spec.wall_color = {155, 150, 140};
	spec.wall_thickness = 0.40;
	spec.roof.shape = "pyramidal";
	spec.roof.height = 6.0;
    } else if (preset == "civic") {
	spec.type = "civic";
	spec.footprint = rectangle(32.0, 20.0);
	spec.levels_above = 3;
	spec.floor_heights.assign(3, 4.0);
	spec.wall_material = "stone";
	spec.wall_color = {185, 180, 165};
	spec.roof.shape = "hipped";
	spec.roof.height = 3.5;
	spec.structure = {"reinforced_concrete_frame", true, 8.0, 5.0, "rectangular", 0.45, 0.45, 0.50, 0.35, 0.65, "reinforced_concrete", {145, 145, 140}};
    } else if (preset == "carport") {
	spec.type = "carport";
	spec.footprint = rectangle(6.5, 6.0);
	spec.levels_above = 1;
	spec.floor_heights = {2.8};
	spec.walls = false;
	spec.auto_openings = false;
	spec.foundation_thickness = 0.18;
	spec.roof.shape = "skillion";
	spec.roof.height = 0.8;
	spec.roof.material = "metal";
	spec.structure = {"post_and_beam", true, 6.0, 5.5, "rectangular", 0.18, 0.18, 0.20, 0.18, 0.30, "wood", {115, 78, 48}};
    } else if (preset == "shed" || preset == "service") {
	spec.type = preset;
	spec.footprint = rectangle(5.0, 4.0);
	spec.levels_above = 1;
	spec.floor_heights = {2.6};
	spec.wall_material = preset == "shed" ? "wood" : "metal";
	spec.wall_color = preset == "shed" ? rgb_color{145, 105, 70} : rgb_color{170, 175, 170};
	spec.roof.shape = "skillion";
	spec.roof.height = 0.8;
    } else {
	throw std::runtime_error("unknown building preset '" + raw_preset + "'");
    }
    return spec;
}

void
reconcile_facade_height(building_spec &spec)
{
    if (spec.total_height <= 0.0 || spec.floor_heights.empty())
	return;
    const double available_height = spec.total_height - effective_roof_height(spec);
    double storey_height = 0.0;
    for (double height : spec.floor_heights)
	storey_height += height;
    if (available_height <= 0.0 || storey_height <= available_height + HEIGHT_CONSISTENCY_EPSILON_METERS)
	return;
    const double scale = available_height / storey_height;
    for (double &height : spec.floor_heights)
	height *= scale;
    spec.metadata["height_reconciliation"] = "storey heights scaled to fit measured height and roof";
}

void
apply_osm_tags(building_spec &spec, const std::map<std::string, std::string> &tags)
{
    spec.osm_tags.insert(tags.begin(), tags.end());
    auto get = [&tags](const char *key) -> const std::string * {
	const auto found = tags.find(key);
	return found == tags.end() ? nullptr : &found->second;
    };
    const std::string *building_value = get("building");
    const std::string *part_value = get("building:part");
    if (building_value)
	spec.type = *building_value;
    else if (part_value)
	spec.type = *part_value;
    set_type_specific_defaults(spec, spec.type);
    const std::string *building_levels = get("building:levels");
    const std::string *levels = get("levels");
    if (building_levels)
	spec.levels_above = parse_osm_integer(*building_levels, "building:levels");
    else if (levels)
	spec.levels_above = parse_osm_integer(*levels, "levels");
    if (const std::string *value = get("building:levels:underground"))
	spec.levels_below = parse_osm_integer(*value, "building:levels:underground");
    if (const std::string *value = get("building:min_level"))
	spec.min_level = parse_osm_integer(*value, "building:min_level");
    const std::string *height = get("height");
    const std::string *building_height = get("building:height");
    if (height)
	spec.total_height = parse_osm_length(*height);
    else if (building_height)
	spec.total_height = parse_osm_length(*building_height);
    const std::string *min_height = get("min_height");
    const std::string *building_min_height = get("building:min_height");
    if (min_height)
	spec.min_height = parse_osm_length(*min_height);
    else if (building_min_height)
	spec.min_height = parse_osm_length(*building_min_height);
    if (const std::string *value = get("building:material"))
	spec.wall_material = *value;
    if (const std::string *value = get("building:colour"))
	spec.wall_color = named_color(*value, spec.wall_color);
    if (const std::string *value = get("building:structure")) {
	spec.structure.system = *value;
	spec.structure.enabled = lower(*value) != "wall_bearing";
    }
    if (const std::string *value = get("building:use"))
	spec.usage = {*value};
    if (const std::string *value = get("roof:shape"))
	spec.roof.shape = normalized_token(*value);
    if (const std::string *value = get("roof:height"))
	spec.roof.height = parse_osm_length(*value);
    if (const std::string *value = get("roof:levels"))
	spec.roof.levels = parse_osm_integer(*value, "roof:levels");
    if (const std::string *value = get("roof:angle"))
	spec.roof.angle_degrees = std::stod(*value);
    if (const std::string *value = get("roof:orientation"))
	spec.roof.orientation = lower(*value);
    if (const std::string *value = get("roof:direction"))
	spec.roof.direction_degrees = std::stod(*value);
    if (const std::string *value = get("roof:material"))
	spec.roof.material = *value;
    if (const std::string *value = get("roof:colour"))
	spec.roof.color = named_color(*value, spec.roof.color);
    if (spec.levels_above > 0)
	spec.floor_heights.assign(static_cast<size_t>(spec.levels_above), spec.floor_heights.empty() ? 3.0 : spec.floor_heights.front());
    if (spec.levels_below > 0)
	spec.basement_heights.assign(static_cast<size_t>(spec.levels_below), 3.0);
}

std::vector<std::string>
string_array(const json &object, const char *key, const std::vector<std::string> &fallback)
{
    if (!object.contains(key))
	return fallback;
    const json &value = object.at(key);
    if (value.is_string())
	return {value.get<std::string>()};
    if (!value.is_array())
	throw std::runtime_error(std::string("'") + key + "' must be a string or array of strings");
    std::vector<std::string> result;
    for (const json &entry : value) {
	if (!entry.is_string())
	    throw std::runtime_error(std::string("'") + key + "' entries must be strings");
	result.push_back(entry.get<std::string>());
    }
    return result;
}

point3
parse_point3(const json &value, double scale, const std::string &context)
{
    if (!value.is_array() || value.size() != 3)
	throw std::runtime_error(context + " must contain three coordinates");
    point3 result;
    double *coordinates[] = {&result.x, &result.y, &result.z};
    for (size_t i = 0; i < 3; ++i) {
	if (!value.at(i).is_number())
	    throw std::runtime_error(context + " coordinates must be numeric");
	*coordinates[i] = value.at(i).get<double>() * scale;
    }
    return result;
}

std::vector<point2>
parse_footprint(const json &value, double scale, const std::string &context)
{
    if (!value.is_array())
	throw std::runtime_error(context + " must be an array of coordinate pairs");
    std::vector<point2> result;
    for (const json &entry : value) {
	if (!entry.is_array() || entry.size() < 2 || !entry.at(0).is_number() || !entry.at(1).is_number())
	    throw std::runtime_error(context + " entries must be numeric [x, y] coordinate pairs");
	result.push_back({entry.at(0).get<double>() * scale, entry.at(1).get<double>() * scale});
    }
    normalize_footprint(result);
    return result;
}

opening_spec
parse_opening(const json &value, double scale, size_t index)
{
    reject_unknown_keys(value,
	{"id", "kind", "type", "wall", "level", "offset", "width", "height", "sill", "count", "spacing",
	 "frame_width", "frame_depth", "material", "color", "osm"}, "opening");
    opening_spec opening;
    opening.id = json_string(value, "id", "opening_" + std::to_string(index));
    opening.kind = lower(json_string(value, "kind", opening.kind));
    if (opening.kind == "window")
	opening.color = {90, 120, 140};
    opening.type = json_string(value, "type", opening.type);
    const int wall = json_integer(value, "wall", 0);
    if (wall < 0)
	throw std::runtime_error("opening wall must not be negative");
    opening.wall = static_cast<size_t>(wall);
    opening.level = json_integer(value, "level", opening.level);
    opening.offset = json_number(value, "offset", opening.offset, scale);
    opening.width = json_number(value, "width", opening.width, scale);
    opening.height = json_number(value, "height", opening.height, scale);
    opening.sill = json_number(value, "sill", opening.kind == "door" ? 0.0 : opening.sill, scale);
    opening.count = json_integer(value, "count", opening.count);
    opening.spacing = json_number(value, "spacing", opening.spacing, scale);
    opening.frame_width = json_number(value, "frame_width", opening.frame_width, scale);
    opening.frame_depth = json_number(value, "frame_depth", opening.frame_depth, scale);
    opening.material = json_string(value, "material", opening.kind == "window" ? "glass" : "wood");
    opening.color = json_color(value, "color", opening.color);
    if (value.contains("osm"))
	opening.osm_tags = string_map(value.at("osm"), "opening osm");
    return opening;
}

installation_spec
parse_installation(const json &value, double scale, size_t index)
{
    reject_unknown_keys(value, {"id", "kind", "shape", "position", "size", "material", "color", "attributes"}, "installation");
    installation_spec result;
    result.id = json_string(value, "id", "installation_" + std::to_string(index));
    result.kind = json_string(value, "kind", result.kind);
    result.shape = lower(json_string(value, "shape", result.shape));
    if (value.contains("position"))
	result.position = parse_point3(value.at("position"), scale, "installation position");
    if (value.contains("size"))
	result.size = parse_point3(value.at("size"), scale, "installation size");
    result.material = json_string(value, "material", result.material);
    result.color = json_color(value, "color", result.color);
    if (value.contains("attributes"))
	result.attributes = string_map(value.at("attributes"), "installation attributes");
    return result;
}

double cityjson_measure(const json &);
building_spec parse_native_object(const json &, const std::string &, const std::string &, bool);

building_spec
parse_native_object(const json &object, const std::string &fallback_id, const std::string &inherited_units, bool is_part)
{
    reject_unknown_keys(object,
	{"$schema", "version", "id", "name", "units", "preset", "type", "class", "function", "usage", "origin",
	 "footprint", "dimensions", "levels", "envelope", "roof", "openings", "auto_openings", "structure",
	 "installations", "parts", "osm", "citygml", "metadata"}, is_part ? "building part" : "building specification");
    if (!is_part) {
	if (!object.contains("version") || !object.at("version").is_number_integer() || object.at("version").get<int>() != 1)
	    throw std::runtime_error("native building specification version must be 1");
    }
    const std::string units = json_string(object, "units", inherited_units.empty() ? "m" : inherited_units);
    const double scale = unit_scale(units);

    std::map<std::string, std::string> osm_tags;
    if (object.contains("osm")) {
	const json &osm = object.at("osm");
	reject_unknown_keys(osm, {"id", "type", "tags"}, "osm");
	if (osm.contains("tags"))
	    osm_tags = string_map(osm.at("tags"), "osm tags");
    }
    std::string type = json_string(object, "type", "");
    if (type.empty()) {
	const auto found = osm_tags.find(is_part ? "building:part" : "building");
	if (found != osm_tags.end())
	    type = found->second;
    }
    const std::string requested_preset = json_string(object, "preset", type.empty() ? "house" : profile_for_type(type));
    building_spec spec = preset_impl(requested_preset);
    spec.id = json_string(object, "id", json_string(object, "name", fallback_id));
    if (!type.empty())
	spec.type = type;
    apply_osm_tags(spec, osm_tags);
    if (!type.empty())
	spec.type = type;
    set_type_specific_defaults(spec, spec.type);

    spec.building_class = json_string(object, "class", spec.building_class);
    spec.function = string_array(object, "function", spec.function);
    spec.usage = string_array(object, "usage", spec.usage);
    if (object.contains("origin"))
	spec.origin = parse_point3(object.at("origin"), scale, "origin");
    if (object.contains("footprint"))
	spec.footprint = parse_footprint(object.at("footprint"), scale, "footprint");
    if (object.contains("dimensions")) {
	const json &dimensions = object.at("dimensions");
	reject_unknown_keys(dimensions, {"width", "depth"}, "dimensions");
	const double width = json_number(dimensions, "width", -1.0, scale);
	const double depth = json_number(dimensions, "depth", -1.0, scale);
	if (width <= 0.0 || depth <= 0.0)
	    throw std::runtime_error("dimensions width and depth must be positive");
	spec.footprint = rectangle(width, depth);
    }
    if (object.contains("levels")) {
	const json &levels = object.at("levels");
	reject_unknown_keys(levels,
	    {"above_ground", "below_ground", "min_level", "floor_height", "floor_heights", "basement_height",
	     "basement_heights", "non_existent"}, "levels");
	spec.levels_above = json_integer(levels, "above_ground", spec.levels_above);
	spec.levels_below = json_integer(levels, "below_ground", spec.levels_below);
	spec.min_level = json_integer(levels, "min_level", spec.min_level);
	if (levels.contains("floor_heights")) {
	    if (!levels.at("floor_heights").is_array())
		throw std::runtime_error("levels floor_heights must be an array");
	    spec.floor_heights.clear();
	    for (const json &height : levels.at("floor_heights")) {
		if (!height.is_number())
		    throw std::runtime_error("levels floor_heights entries must be numeric");
		spec.floor_heights.push_back(height.get<double>() * scale);
	    }
	} else {
	    const double floor_height = json_number(levels, "floor_height", spec.floor_heights.empty() ? 3.0 : spec.floor_heights.front(), scale);
	    spec.floor_heights.assign(static_cast<size_t>(std::max(0, spec.levels_above)), floor_height);
	}
	if (levels.contains("basement_heights")) {
	    if (!levels.at("basement_heights").is_array())
		throw std::runtime_error("levels basement_heights must be an array");
	    spec.basement_heights.clear();
	    for (const json &height : levels.at("basement_heights")) {
		if (!height.is_number())
		    throw std::runtime_error("levels basement_heights entries must be numeric");
		spec.basement_heights.push_back(height.get<double>() * scale);
	    }
	} else {
	    const double basement_height = json_number(levels, "basement_height", 3.0, scale);
	    spec.basement_heights.assign(static_cast<size_t>(std::max(0, spec.levels_below)), basement_height);
	}
	if (levels.contains("non_existent")) {
	    if (!levels.at("non_existent").is_array())
		throw std::runtime_error("levels non_existent must be an array");
	    spec.non_existent_levels.clear();
	    for (const json &level : levels.at("non_existent")) {
		if (!level.is_number_integer())
		    throw std::runtime_error("levels non_existent entries must be integers");
		spec.non_existent_levels.push_back(level.get<int>());
	    }
	}
    }
    if (object.contains("envelope")) {
	const json &envelope = object.at("envelope");
	reject_unknown_keys(envelope,
	    {"height", "min_height", "walls", "wall_indices", "wall_thickness", "slab_thickness", "foundation_thickness",
	     "material", "color"}, "envelope");
	spec.total_height = json_number(envelope, "height", spec.total_height, scale);
	spec.min_height = json_number(envelope, "min_height", spec.min_height, scale);
	spec.walls = json_boolean(envelope, "walls", spec.walls);
	spec.wall_thickness = json_number(envelope, "wall_thickness", spec.wall_thickness, scale);
	spec.slab_thickness = json_number(envelope, "slab_thickness", spec.slab_thickness, scale);
	spec.foundation_thickness = json_number(envelope, "foundation_thickness", spec.foundation_thickness, scale);
	spec.wall_material = json_string(envelope, "material", spec.wall_material);
	spec.wall_color = json_color(envelope, "color", spec.wall_color);
	if (envelope.contains("wall_indices")) {
	    if (!envelope.at("wall_indices").is_array())
		throw std::runtime_error("envelope wall_indices must be an array");
	    spec.wall_indices.clear();
	    for (const json &wall : envelope.at("wall_indices")) {
		if (!wall.is_number_unsigned() && !wall.is_number_integer())
		    throw std::runtime_error("envelope wall_indices entries must be non-negative integers");
		const int wall_index = wall.get<int>();
		if (wall_index < 0)
		    throw std::runtime_error("envelope wall_indices entries must be non-negative");
		spec.wall_indices.push_back(static_cast<size_t>(wall_index));
	    }
	}
    }
    if (object.contains("roof")) {
	const json &roof = object.at("roof");
	reject_unknown_keys(roof,
	    {"shape", "height", "angle", "levels", "orientation", "direction", "overhang", "thickness", "material", "color"}, "roof");
	spec.roof.shape = normalized_token(json_string(roof, "shape", spec.roof.shape));
	spec.roof.height = json_number(roof, "height", spec.roof.height, scale);
	spec.roof.angle_degrees = json_number(roof, "angle", spec.roof.angle_degrees);
	spec.roof.levels = json_integer(roof, "levels", spec.roof.levels);
	spec.roof.orientation = lower(json_string(roof, "orientation", spec.roof.orientation));
	spec.roof.direction_degrees = json_number(roof, "direction", spec.roof.direction_degrees);
	spec.roof.overhang = json_number(roof, "overhang", spec.roof.overhang, scale);
	spec.roof.thickness = json_number(roof, "thickness", spec.roof.thickness, scale);
	spec.roof.material = json_string(roof, "material", spec.roof.material);
	spec.roof.color = json_color(roof, "color", spec.roof.color);
    }
    spec.auto_openings = json_boolean(object, "auto_openings", spec.auto_openings);
    if (object.contains("openings")) {
	if (!object.at("openings").is_array())
	    throw std::runtime_error("openings must be an array");
	spec.openings.clear();
	for (size_t i = 0; i < object.at("openings").size(); ++i)
	    spec.openings.push_back(parse_opening(object.at("openings").at(i), scale, i));
    }
    if (object.contains("structure")) {
	const json &structure = object.at("structure");
	reject_unknown_keys(structure,
	    {"enabled", "system", "grid", "column_shape", "column_width", "column_depth", "column_diameter",
	     "beam_width", "beam_depth", "material", "color"}, "structure");
	spec.structure.enabled = json_boolean(structure, "enabled", spec.structure.enabled);
	spec.structure.system = json_string(structure, "system", spec.structure.system);
	if (structure.contains("grid")) {
	    if (!structure.at("grid").is_array() || structure.at("grid").size() != 2 ||
		!structure.at("grid").at(0).is_number() || !structure.at("grid").at(1).is_number())
		throw std::runtime_error("structure grid must be [x_spacing, y_spacing]");
	    spec.structure.grid_x = structure.at("grid").at(0).get<double>() * scale;
	    spec.structure.grid_y = structure.at("grid").at(1).get<double>() * scale;
	}
	spec.structure.column_shape = lower(json_string(structure, "column_shape", spec.structure.column_shape));
	spec.structure.column_width = json_number(structure, "column_width", spec.structure.column_width, scale);
	spec.structure.column_depth = json_number(structure, "column_depth", spec.structure.column_depth, scale);
	spec.structure.column_diameter = json_number(structure, "column_diameter", spec.structure.column_diameter, scale);
	spec.structure.beam_width = json_number(structure, "beam_width", spec.structure.beam_width, scale);
	spec.structure.beam_depth = json_number(structure, "beam_depth", spec.structure.beam_depth, scale);
	spec.structure.material = json_string(structure, "material", spec.structure.material);
	spec.structure.color = json_color(structure, "color", spec.structure.color);
    }
    if (object.contains("installations")) {
	if (!object.at("installations").is_array())
	    throw std::runtime_error("installations must be an array");
	spec.installations.clear();
	for (size_t i = 0; i < object.at("installations").size(); ++i)
	    spec.installations.push_back(parse_installation(object.at("installations").at(i), scale, i));
    }
    if (object.contains("citygml")) {
	const json &citygml = object.at("citygml");
	reject_unknown_keys(citygml,
	    {"class", "function", "usage", "roofType", "measuredHeight", "storeysAboveGround", "storeysBelowGround",
	     "storeyHeightsAboveGround", "storeyHeightsBelowGround", "attributes"}, "citygml");
	if (citygml.contains("class"))
	    spec.building_class = value_string(citygml.at("class"));
	if (citygml.contains("function"))
	    spec.function = string_array(citygml, "function", spec.function);
	if (citygml.contains("usage"))
	    spec.usage = string_array(citygml, "usage", spec.usage);
	if (citygml.contains("roofType"))
	    spec.roof.shape = normalized_token(value_string(citygml.at("roofType")));
	if (citygml.contains("measuredHeight"))
	    spec.total_height = citygml.at("measuredHeight").is_number() ? citygml.at("measuredHeight").get<double>() * scale : cityjson_measure(citygml.at("measuredHeight"));
	if (citygml.contains("storeysAboveGround"))
	    spec.levels_above = json_integer(citygml, "storeysAboveGround", spec.levels_above);
	if (citygml.contains("storeysBelowGround"))
	    spec.levels_below = json_integer(citygml, "storeysBelowGround", spec.levels_below);
	if (citygml.contains("storeyHeightsAboveGround")) {
	    const json &heights = citygml.at("storeyHeightsAboveGround");
	    if (!heights.is_array())
		throw std::runtime_error("citygml storeyHeightsAboveGround must be an array");
	    spec.floor_heights.clear();
	    for (const json &height : heights)
		spec.floor_heights.push_back(height.is_number() ? height.get<double>() * scale : cityjson_measure(height));
	} else {
	    spec.floor_heights.assign(static_cast<size_t>(std::max(0, spec.levels_above)), spec.floor_heights.empty() ? 3.0 * scale : spec.floor_heights.front());
	}
	if (citygml.contains("storeyHeightsBelowGround")) {
	    const json &heights = citygml.at("storeyHeightsBelowGround");
	    if (!heights.is_array())
		throw std::runtime_error("citygml storeyHeightsBelowGround must be an array");
	    spec.basement_heights.clear();
	    for (const json &height : heights)
		spec.basement_heights.push_back(height.is_number() ? height.get<double>() * scale : cityjson_measure(height));
	} else {
	    spec.basement_heights.assign(static_cast<size_t>(std::max(0, spec.levels_below)), 3.0 * scale);
	}
	if (citygml.contains("attributes"))
	    spec.cityjson_attributes = string_map(citygml.at("attributes"), "citygml attributes");
    }
    if (object.contains("metadata"))
	spec.metadata = string_map(object.at("metadata"), "metadata");
    if (object.contains("parts")) {
	if (!object.at("parts").is_array())
	    throw std::runtime_error("parts must be an array");
	spec.parts.clear();
	for (size_t i = 0; i < object.at("parts").size(); ++i) {
	    building_spec part = parse_native_object(object.at("parts").at(i), spec.id + "_part_" + std::to_string(i), units, true);
	    part.origin.x += spec.origin.x;
	    part.origin.y += spec.origin.y;
	    part.origin.z += spec.origin.z;
	spec.parts.push_back(std::move(part));
	}
    }
    reconcile_facade_height(spec);
    return spec;
}

std::map<std::string, std::string>
geojson_tags(const json &properties)
{
    if (!properties.is_object())
	throw std::runtime_error("GeoJSON properties must be an object");
    if (properties.contains("tags") && properties.at("tags").is_object())
	return string_map(properties.at("tags"), "GeoJSON properties tags");
    return string_map(properties, "GeoJSON properties");
}

std::vector<point2>
geographic_ring(const json &ring)
{
    if (!ring.is_array() || ring.size() < 4)
	throw std::runtime_error("geographic footprint ring must have at least four positions");
    const double lon0 = ring.at(0).at(0).get<double>();
    const double lat0 = ring.at(0).at(1).get<double>();
    const double cos_lat = std::cos(lat0 * PI / 180.0);
    std::vector<point2> result;
    for (const json &position : ring) {
	if (!position.is_array() || position.size() < 2 || !position.at(0).is_number() || !position.at(1).is_number())
	    throw std::runtime_error("geographic footprint positions must be [longitude, latitude]");
	const double lon = position.at(0).get<double>();
	const double lat = position.at(1).get<double>();
	result.push_back({EARTH_RADIUS_METERS * cos_lat * (lon - lon0) * PI / 180.0,
	    EARTH_RADIUS_METERS * (lat - lat0) * PI / 180.0});
    }
    normalize_footprint(result);
    return result;
}

building_spec
parse_geojson_feature(const json &feature, const std::string &fallback_id)
{
    if (!feature.is_object() || json_string(feature, "type", "") != "Feature")
	throw std::runtime_error("GeoJSON entry must be a Feature");
    if (!feature.contains("properties") || !feature.contains("geometry"))
	throw std::runtime_error("GeoJSON Feature must contain properties and geometry");
    const std::map<std::string, std::string> tags = geojson_tags(feature.at("properties"));
    std::string type = "yes";
    const auto type_tag = tags.find("building");
    if (type_tag != tags.end())
	type = type_tag->second;
    else if ((tags.find("building:part")) != tags.end())
	type = tags.at("building:part");
    building_spec spec = preset_impl(profile_for_type(type));
    spec.type = type;
    spec.id = feature.contains("id") ? value_string(feature.at("id")) : fallback_id;
    apply_osm_tags(spec, tags);
    const json &geometry = feature.at("geometry");
    const std::string geometry_type = json_string(geometry, "type", "");
    if (geometry_type != "Polygon")
	throw std::runtime_error("GeoJSON building geometry must be a Polygon; MultiPolygon buildings should be split into parts");
    if (!geometry.contains("coordinates") || !geometry.at("coordinates").is_array() || geometry.at("coordinates").empty())
	throw std::runtime_error("GeoJSON Polygon has no exterior ring");
    spec.footprint = geographic_ring(geometry.at("coordinates").at(0));
    if (feature.contains("properties")) {
	const json &properties = feature.at("properties");
	if (properties.contains("name") && properties.at("name").is_string())
	spec.id = properties.at("name").get<std::string>();
    }
    reconcile_facade_height(spec);
    return spec;
}

std::vector<building_spec>
parse_geojson(const json &root, const std::string &fallback_id)
{
    std::vector<building_spec> result;
    const std::string type = json_string(root, "type", "");
    if (type == "Feature") {
	result.push_back(parse_geojson_feature(root, fallback_id));
    } else if (type == "FeatureCollection") {
	if (!root.contains("features") || !root.at("features").is_array())
	    throw std::runtime_error("GeoJSON FeatureCollection must contain a features array");
	for (size_t i = 0; i < root.at("features").size(); ++i)
	    result.push_back(parse_geojson_feature(root.at("features").at(i), fallback_id + "_" + std::to_string(i)));
    } else {
	throw std::runtime_error("unsupported GeoJSON root type '" + type + "'");
    }
    return result;
}

std::vector<building_spec>
parse_overpass(const json &root, const std::string &fallback_id)
{
    if (!root.at("elements").is_array())
	throw std::runtime_error("Overpass elements must be an array");
    std::vector<building_spec> result;
    for (const json &element : root.at("elements")) {
	if (!element.is_object() || !element.contains("tags") || !element.at("tags").is_object() || !element.contains("geometry"))
	    continue;
	const auto tags = string_map(element.at("tags"), "Overpass tags");
	if (tags.find("building") == tags.end() && tags.find("building:part") == tags.end())
	    continue;
	json ring = json::array();
	for (const json &node : element.at("geometry")) {
	    if (!node.contains("lon") || !node.contains("lat"))
		throw std::runtime_error("Overpass geometry nodes must contain lon and lat");
	    ring.push_back({node.at("lon"), node.at("lat")});
	}
	json feature = {
	    {"type", "Feature"},
	    {"id", element.contains("id") ? value_string(element.at("id")) : fallback_id + "_" + std::to_string(result.size())},
	    {"properties", element.at("tags")},
	    {"geometry", {{"type", "Polygon"}, {"coordinates", json::array({ring})}}}
	};
	result.push_back(parse_geojson_feature(feature, fallback_id + "_" + std::to_string(result.size())));
    }
    if (result.empty())
	throw std::runtime_error("Overpass input contains no building ways with inline geometry");
    return result;
}

double
cityjson_measure(const json &value)
{
    if (value.is_number())
	return value.get<double>();
    if (value.is_object() && value.contains("value") && value.at("value").is_number()) {
	double result = value.at("value").get<double>();
	if (value.contains("uom") && value.at("uom").is_string())
	    result *= unit_scale(value.at("uom").get<std::string>());
	return result;
    }
    throw std::runtime_error("CityJSON measure must be numeric or a {value, uom} object");
}

void
collect_vertex_indices(const json &value, std::vector<size_t> &indices)
{
    if (value.is_number_unsigned() || value.is_number_integer()) {
	const long long index = value.get<long long>();
	if (index >= 0)
	    indices.push_back(static_cast<size_t>(index));
	return;
    }
    if (value.is_array()) {
	for (const json &entry : value)
	    collect_vertex_indices(entry, indices);
    }
}

building_spec
parse_cityjson_object(
    const std::string &id,
    const json &object,
    const json &root,
    const std::vector<point3> &vertices)
{
    const json attributes = object.contains("attributes") && object.at("attributes").is_object() ? object.at("attributes") : json::object();
    std::string type = "yes";
    if (attributes.contains("buildingType"))
	type = value_string(attributes.at("buildingType"));
    else if (attributes.contains("function")) {
	const json &function = attributes.at("function");
	type = function.is_array() && !function.empty() ? value_string(function.at(0)) : value_string(function);
    }
    building_spec spec = preset_impl(profile_for_type(type));
    spec.id = id;
    spec.type = type;
    set_type_specific_defaults(spec, type);
    spec.cityjson_attributes = string_map(attributes, "CityJSON attributes");
    spec.metadata["cityjson_type"] = json_string(object, "type", "Building");
    spec.metadata["cityjson_version"] = json_string(root, "version", "2.0");
    if (root.contains("metadata") && root.at("metadata").is_object() && root.at("metadata").contains("referenceSystem"))
	spec.metadata["referenceSystem"] = value_string(root.at("metadata").at("referenceSystem"));
    if (attributes.contains("class"))
	spec.building_class = value_string(attributes.at("class"));
    if (attributes.contains("function")) {
	if (attributes.at("function").is_array()) {
	    for (const json &entry : attributes.at("function"))
		spec.function.push_back(value_string(entry));
	} else {
	    spec.function.push_back(value_string(attributes.at("function")));
	}
    }
    if (attributes.contains("usage")) {
	if (attributes.at("usage").is_array()) {
	    for (const json &entry : attributes.at("usage"))
		spec.usage.push_back(value_string(entry));
	} else {
	    spec.usage.push_back(value_string(attributes.at("usage")));
	}
    }
    if (attributes.contains("roofType"))
	spec.roof.shape = normalized_token(value_string(attributes.at("roofType")));
    if (attributes.contains("measuredHeight"))
	spec.total_height = cityjson_measure(attributes.at("measuredHeight"));
    if (attributes.contains("storeysAboveGround") && attributes.at("storeysAboveGround").is_number_integer())
	spec.levels_above = attributes.at("storeysAboveGround").get<int>();
    if (attributes.contains("storeysBelowGround") && attributes.at("storeysBelowGround").is_number_integer())
	spec.levels_below = attributes.at("storeysBelowGround").get<int>();
    if (attributes.contains("storeyHeightsAboveGround") && attributes.at("storeyHeightsAboveGround").is_array()) {
	spec.floor_heights.clear();
	for (const json &height : attributes.at("storeyHeightsAboveGround"))
	    spec.floor_heights.push_back(cityjson_measure(height));
    } else {
	spec.floor_heights.assign(static_cast<size_t>(std::max(0, spec.levels_above)), spec.floor_heights.empty() ? 3.0 : spec.floor_heights.front());
    }
    if (attributes.contains("storeyHeightsBelowGround") && attributes.at("storeyHeightsBelowGround").is_array()) {
	spec.basement_heights.clear();
	for (const json &height : attributes.at("storeyHeightsBelowGround"))
	    spec.basement_heights.push_back(cityjson_measure(height));
    } else {
	spec.basement_heights.assign(static_cast<size_t>(std::max(0, spec.levels_below)), 3.0);
    }

    double min_x = std::numeric_limits<double>::infinity();
    double min_y = std::numeric_limits<double>::infinity();
    double min_z = std::numeric_limits<double>::infinity();
    double max_x = -std::numeric_limits<double>::infinity();
    double max_y = -std::numeric_limits<double>::infinity();
    double max_z = -std::numeric_limits<double>::infinity();
    auto accumulate = [&](const point3 &p) {
	min_x = std::min(min_x, p.x); min_y = std::min(min_y, p.y); min_z = std::min(min_z, p.z);
	max_x = std::max(max_x, p.x); max_y = std::max(max_y, p.y); max_z = std::max(max_z, p.z);
    };
    if (object.contains("geographicalExtent") && object.at("geographicalExtent").is_array() && object.at("geographicalExtent").size() == 6) {
	const json &extent = object.at("geographicalExtent");
	accumulate({extent.at(0).get<double>(), extent.at(1).get<double>(), extent.at(2).get<double>()});
	accumulate({extent.at(3).get<double>(), extent.at(4).get<double>(), extent.at(5).get<double>()});
    } else if (object.contains("geometry") && object.at("geometry").is_array()) {
	std::vector<size_t> indices;
	for (const json &geometry : object.at("geometry")) {
	    if (geometry.contains("boundaries"))
		collect_vertex_indices(geometry.at("boundaries"), indices);
	}
	for (size_t index : indices) {
	    if (index >= vertices.size())
		throw std::runtime_error("CityJSON geometry references an invalid vertex index");
	    accumulate(vertices[index]);
	}
    }
    if (std::isfinite(min_x) && max_x > min_x && max_y > min_y) {
	spec.origin = {min_x, min_y, min_z};
	spec.footprint = rectangle(max_x - min_x, max_y - min_y);
	if (spec.total_height < 0.0 && max_z > min_z)
	    spec.total_height = max_z - min_z;
    }
    reconcile_facade_height(spec);
    return spec;
}

std::vector<building_spec>
parse_cityjson(const json &root)
{
    const std::string version = json_string(root, "version", "");
    if (version.rfind("2.", 0) != 0)
	throw std::runtime_error("CityJSON input must use version 2.x");
    if (!root.contains("CityObjects") || !root.at("CityObjects").is_object())
	throw std::runtime_error("CityJSON input must contain a CityObjects object");
    std::array<double, 3> scale = {1.0, 1.0, 1.0};
    std::array<double, 3> translate = {0.0, 0.0, 0.0};
    if (root.contains("transform")) {
	const json &transform = root.at("transform");
	if (!transform.contains("scale") || !transform.contains("translate") || transform.at("scale").size() != 3 || transform.at("translate").size() != 3)
	    throw std::runtime_error("CityJSON transform must contain three-element scale and translate arrays");
	for (size_t i = 0; i < 3; ++i) {
	    scale[i] = transform.at("scale").at(i).get<double>();
	    translate[i] = transform.at("translate").at(i).get<double>();
	}
    }
    std::vector<point3> vertices;
    if (root.contains("vertices")) {
	if (!root.at("vertices").is_array())
	    throw std::runtime_error("CityJSON vertices must be an array");
	for (const json &vertex : root.at("vertices")) {
	    if (!vertex.is_array() || vertex.size() != 3)
		throw std::runtime_error("CityJSON vertex must contain three coordinates");
	    vertices.push_back({vertex.at(0).get<double>() * scale[0] + translate[0],
		vertex.at(1).get<double>() * scale[1] + translate[1],
		vertex.at(2).get<double>() * scale[2] + translate[2]});
	}
    }
    std::map<std::string, building_spec> parsed;
    for (auto it = root.at("CityObjects").begin(); it != root.at("CityObjects").end(); ++it) {
	const std::string object_type = json_string(it.value(), "type", "");
	if (object_type == "Building" || object_type == "BuildingPart")
	    parsed.emplace(it.key(), parse_cityjson_object(it.key(), it.value(), root, vertices));
    }
    std::vector<building_spec> result;
    for (auto &entry : parsed) {
	const json &object = root.at("CityObjects").at(entry.first);
	if (json_string(object, "type", "") != "Building")
	    continue;
	building_spec parent = entry.second;
	if (object.contains("children") && object.at("children").is_array()) {
	    for (const json &child_id_json : object.at("children")) {
		if (!child_id_json.is_string())
		    continue;
		const auto child = parsed.find(child_id_json.get<std::string>());
		if (child != parsed.end())
		    parent.parts.push_back(child->second);
	    }
	}
	result.push_back(std::move(parent));
    }
    if (result.empty())
	throw std::runtime_error("CityJSON input contains no Building objects");
    return result;
}

double
orientation_value(const point2 &a, const point2 &b, const point2 &c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool
segments_intersect(const point2 &a, const point2 &b, const point2 &c, const point2 &d)
{
    constexpr double epsilon = 1.0e-10;
    const double o1 = orientation_value(a, b, c);
    const double o2 = orientation_value(a, b, d);
    const double o3 = orientation_value(c, d, a);
    const double o4 = orientation_value(c, d, b);
    return ((o1 > epsilon && o2 < -epsilon) || (o1 < -epsilon && o2 > epsilon)) &&
	((o3 > epsilon && o4 < -epsilon) || (o3 < -epsilon && o4 > epsilon));
}

double
level_base(const building_spec &spec, int level)
{
    if (level >= 0) {
	double z = 0.0;
	for (int i = 0; i < level && i < static_cast<int>(spec.floor_heights.size()); ++i)
	    z += spec.floor_heights[static_cast<size_t>(i)];
	return z;
    }
    double z = 0.0;
    for (int i = 0; i > level && static_cast<size_t>(-i - 1) < spec.basement_heights.size(); --i)
	z -= spec.basement_heights[static_cast<size_t>(-i - 1)];
    return z;
}

json
color_json(rgb_color color)
{
    return json::array({color.r, color.g, color.b});
}

json spec_to_json(const building_spec &);

json
spec_to_json(const building_spec &spec)
{
    json result;
    result["version"] = spec.version;
    result["id"] = spec.id;
    result["units"] = "m";
    result["type"] = spec.type;
    result["class"] = spec.building_class;
    result["function"] = spec.function;
    result["usage"] = spec.usage;
    result["origin"] = {spec.origin.x, spec.origin.y, spec.origin.z};
    result["footprint"] = json::array();
    for (const point2 &point : spec.footprint)
	result["footprint"].push_back({point.x, point.y});
    result["levels"] = {
	{"above_ground", spec.levels_above}, {"below_ground", spec.levels_below}, {"min_level", spec.min_level},
	{"floor_heights", spec.floor_heights}, {"basement_heights", spec.basement_heights},
	{"non_existent", spec.non_existent_levels}
    };
    result["envelope"] = {
	{"walls", spec.walls},
	{"wall_indices", spec.wall_indices}, {"wall_thickness", spec.wall_thickness},
	{"slab_thickness", spec.slab_thickness}, {"foundation_thickness", spec.foundation_thickness},
	{"material", spec.wall_material}, {"color", color_json(spec.wall_color)}
    };
    if (spec.total_height > 0.0)
	result["envelope"]["height"] = spec.total_height;
    if (spec.min_height >= 0.0)
	result["envelope"]["min_height"] = spec.min_height;
    result["roof"] = {
	{"shape", spec.roof.shape}, {"height", spec.roof.height},
	{"levels", spec.roof.levels}, {"orientation", spec.roof.orientation},
	{"overhang", spec.roof.overhang}, {"thickness", spec.roof.thickness},
	{"material", spec.roof.material}, {"color", color_json(spec.roof.color)}
    };
    if (spec.roof.angle_degrees >= 0.0)
	result["roof"]["angle"] = spec.roof.angle_degrees;
    if (spec.roof.direction_degrees >= 0.0)
	result["roof"]["direction"] = spec.roof.direction_degrees;
    result["auto_openings"] = spec.auto_openings;
    result["openings"] = json::array();
    for (const opening_spec &opening : spec.openings) {
	result["openings"].push_back({
	    {"id", opening.id}, {"kind", opening.kind}, {"type", opening.type}, {"wall", opening.wall},
	    {"level", opening.level}, {"offset", opening.offset}, {"width", opening.width},
	    {"height", opening.height}, {"sill", opening.sill}, {"count", opening.count},
	    {"spacing", opening.spacing}, {"frame_width", opening.frame_width}, {"frame_depth", opening.frame_depth},
	    {"material", opening.material}, {"color", color_json(opening.color)}, {"osm", opening.osm_tags}
	});
    }
    result["structure"] = {
	{"enabled", spec.structure.enabled}, {"system", spec.structure.system},
	{"grid", {spec.structure.grid_x, spec.structure.grid_y}}, {"column_shape", spec.structure.column_shape},
	{"column_width", spec.structure.column_width}, {"column_depth", spec.structure.column_depth},
	{"column_diameter", spec.structure.column_diameter}, {"beam_width", spec.structure.beam_width},
	{"beam_depth", spec.structure.beam_depth}, {"material", spec.structure.material},
	{"color", color_json(spec.structure.color)}
    };
    result["installations"] = json::array();
    for (const installation_spec &installation : spec.installations) {
	result["installations"].push_back({
	    {"id", installation.id}, {"kind", installation.kind}, {"shape", installation.shape},
	    {"position", {installation.position.x, installation.position.y, installation.position.z}},
	    {"size", {installation.size.x, installation.size.y, installation.size.z}},
	    {"material", installation.material}, {"color", color_json(installation.color)},
	    {"attributes", installation.attributes}
	});
    }
    result["parts"] = json::array();
    for (const building_spec &part : spec.parts)
	result["parts"].push_back(spec_to_json(part));
    result["osm"] = {{"tags", spec.osm_tags}};
    result["citygml"] = {
	{"class", spec.building_class}, {"function", spec.function}, {"usage", spec.usage},
	{"roofType", spec.roof.shape}, {"measuredHeight", spec.total_height},
	{"storeysAboveGround", spec.levels_above}, {"storeysBelowGround", spec.levels_below},
	{"storeyHeightsAboveGround", spec.floor_heights}, {"storeyHeightsBelowGround", spec.basement_heights},
	{"attributes", spec.cityjson_attributes}
    };
    result["metadata"] = spec.metadata;
    return result;
}

std::string
realized_roof_shape(const building_spec &spec)
{
    std::string shape = normalized_token(spec.roof.shape);
    if (effective_roof_height(spec) <= spec.roof.thickness + HEIGHT_CONSISTENCY_EPSILON_METERS ||
	shape == "flat" || shape == "many")
	return "flat";

    const std::set<std::string> profile_shapes = {
	"gabled", "gabled_height_moved", "skillion", "saltbox", "gambrel", "bellcast_gable",
	"mansard", "equal_mansard", "butterfly", "sawtooth", "round", "round_gabled",
	"parabolic", "pitched"
    };
    const std::set<std::string> hipped_shapes = {
	"hipped", "half_hipped", "side_hipped", "side_half_hipped", "hipped_and_gabled", "equal_hipped"
    };
    if (spec.footprint.size() != 4 && (profile_shapes.count(shape) || hipped_shapes.count(shape)))
	return "pyramidal";
    if (shape == "crosspitched" || shape == "apse_gabled" || shape == "pyramidal")
	return "pyramidal";
    if (hipped_shapes.count(shape))
	return "hipped";
    if (shape == "half_dome")
	return "dome";
    if (shape == "pitched")
	return "gabled";
    if (shape == "gabled_height_moved")
	return "saltbox";
    if (shape == "bellcast_gable")
	return "gambrel";
    if (shape == "equal_mansard")
	return "mansard";
    if (shape == "round_gabled")
	return "round";
    return shape;
}

std::string
automatic_opening_geometry(const building_spec &spec)
{
    if (spec.type == "garage" || spec.type == "garages")
	return "overhead_door";
    if (spec.type == "office")
	return "office_windows";
    if (spec.type == "church")
	return "church_windows";
    if (spec.type == "warehouse" || spec.type == "barn" || spec.type == "sports_hall")
	return "sparse_windows";
    return "standard_windows";
}

void
erase_keys(json &object, std::initializer_list<const char *> keys)
{
    for (const char *key : keys)
	object.erase(key);
}

json
geometry_signature_json(const building_spec &spec, const point3 &reference_origin)
{
    json result = spec_to_json(spec);
    erase_keys(result, {"version", "id", "units", "type", "class", "function", "usage", "osm", "citygml", "metadata"});
    result["origin"] = {
	spec.origin.x - reference_origin.x,
	spec.origin.y - reference_origin.y,
	spec.origin.z - reference_origin.z
    };

    json &envelope = result["envelope"];
    erase_keys(envelope, {"material", "color"});
    if (!spec.walls)
	erase_keys(envelope, {"wall_indices", "wall_thickness"});

    json &roof = result["roof"];
    const std::string roof_shape = realized_roof_shape(spec);
    roof["shape"] = roof_shape;
    roof["effective_height"] = effective_roof_height(spec);
    if (roof_shape == "flat")
	roof["thickness"] = effective_roof_height(spec);
    erase_keys(roof, {"height", "levels", "angle", "material", "color"});
    const bool oriented_roof = roof_shape == "hipped" || roof_shape == "gabled" ||
	roof_shape == "saltbox" || roof_shape == "skillion" || roof_shape == "gambrel" ||
	roof_shape == "mansard" || roof_shape == "butterfly" || roof_shape == "sawtooth" ||
	roof_shape == "round" || roof_shape == "parabolic";
    if (!oriented_roof)
	erase_keys(roof, {"orientation", "direction"});

    result.erase("auto_openings");
    if (spec.auto_openings && spec.openings.empty() && spec.walls)
	result["automatic_opening_geometry"] = automatic_opening_geometry(spec);
    for (json &opening : result["openings"])
	erase_keys(opening, {"id", "type", "material", "color", "osm"});

    json &structure = result["structure"];
    if (!spec.structure.enabled) {
	structure = {{"enabled", false}};
    } else {
	erase_keys(structure, {"system", "material", "color"});
	if (spec.structure.column_shape == "round")
	    erase_keys(structure, {"column_width", "column_depth"});
	else
	    structure.erase("column_diameter");
    }

    for (json &installation : result["installations"])
	erase_keys(installation, {"id", "kind", "material", "color", "attributes"});

    result["parts"] = json::array();
    for (const building_spec &part : spec.parts)
	result["parts"].push_back(geometry_signature_json(part, reference_origin));
    return result;
}

std::string
geometry_signature(const building_spec &spec)
{
    return geometry_signature_json(spec, spec.origin).dump();
}

void
require_unique_demo_geometry(const std::vector<building_spec> &specs)
{
    std::map<std::string, std::string> signatures;
    for (const building_spec &spec : specs) {
	const std::string signature = geometry_signature(spec);
	const auto inserted = signatures.emplace(signature, spec.type);
	if (!inserted.second)
	    throw std::runtime_error("demo building types '" + inserted.first->second + "' and '" +
		spec.type + "' generate identical shape and structure");
    }
}

} // namespace

double
effective_roof_height(const building_spec &spec)
{
    double height = std::max(spec.roof.height, spec.roof.thickness);
    if (spec.roof.levels > 0)
	height = std::max(height, static_cast<double>(spec.roof.levels) * 3.0);
    if (spec.roof.angle_degrees > 0.0) {
	double shortest_edge = std::numeric_limits<double>::infinity();
	for (size_t i = 0; i < spec.footprint.size(); ++i) {
	    const point2 &a = spec.footprint[i];
	    const point2 &b = spec.footprint[(i + 1) % spec.footprint.size()];
	    shortest_edge = std::min(shortest_edge, std::hypot(b.x - a.x, b.y - a.y));
	}
	height = std::max(spec.roof.thickness,
	    std::tan(spec.roof.angle_degrees * PI / 180.0) * shortest_edge * 0.5);
    }
    return height;
}

building_spec
make_preset(const std::string &preset)
{
    try {
	return preset_impl(preset);
    } catch (const std::runtime_error &) {
	building_spec spec = preset_impl(profile_for_type(preset));
	set_type_specific_defaults(spec, preset);
	spec.id = normalized_token(preset);
	return spec;
    }
}

std::vector<building_spec>
make_demo_specs()
{
    struct demo_entry { const char *type; const char *roof; };
    const demo_entry entries[] = {
	{"house", "gabled"}, {"bungalow", "hipped"}, {"apartments", "flat"}, {"hotel", "mansard"},
	{"dormitory", "gabled_height_moved"}, {"stilt_house", "side_hipped"}, {"tree_house", "pyramidal"},
	{"trullo", "cone"}, {"industrial", "sawtooth"}, {"warehouse", "gabled"}, {"hangar", "round_gabled"},
	{"office", "skillion"}, {"retail", "flat"}, {"supermarket", "butterfly"}, {"kiosk", "equal_hipped"},
	{"church", "crosspitched"}, {"cathedral", "apse_gabled"}, {"mosque", "dome"},
	{"synagogue", "half-dome"}, {"temple", "equal_mansard"}, {"pagoda", "pyramidal"},
	{"civic", "hipped-and-gabled"}, {"school", "half-hipped"}, {"hospital", "side_half-hipped"},
	{"museum", "many"}, {"train_station", "bellcast_gable"}, {"fire_station", "gabled"},
	{"barn", "gambrel"}, {"cowshed", "saltbox"}, {"greenhouse", "round"}, {"stable", "pitched"},
	{"sports_hall", "parabolic"}, {"stadium", "dome"}, {"grandstand", "skillion"},
	{"riding_hall", "round_gabled"}, {"garage", "gabled"}, {"parking", "flat"},
	{"carport", "skillion"}, {"shed", "saltbox"}, {"boathouse", "gabled"}, {"silo", "cone"},
	{"storage_tank", "dome"}, {"water_tower", "onion"}, {"digester", "dome"},
	{"tower", "pyramidal"}, {"clock_tower", "equal_hipped"}, {"bunker", "flat"},
	{"container", "flat"}, {"quonset_hut", "round"}, {"windmill", "cone"}
    };
    std::vector<building_spec> result;
    constexpr size_t columns = 5;
    constexpr double x_spacing = 75.0;
    constexpr double y_spacing = 70.0;
    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); ++i) {
	building_spec spec = make_preset(entries[i].type);
	spec.id = std::string("demo_") + entries[i].type;
	spec.roof.shape = entries[i].roof;
	spec.origin.x = static_cast<double>(i % columns) * x_spacing;
	spec.origin.y = static_cast<double>(i / columns) * y_spacing;
	spec.metadata["demo_index"] = std::to_string(i + 1);
	result.push_back(std::move(spec));
    }
    building_spec parts = preset_impl("office");
    parts.id = "demo_parts";
    parts.origin = {static_cast<double>(columns) * x_spacing, 0.0, 0.0};
    parts.footprint = rectangle(18.0, 18.0);
    parts.levels_above = 3;
    parts.floor_heights.assign(3, 3.6);
    parts.roof.shape = "flat";
    building_spec tower_part = preset_impl("tower");
    tower_part.id = "demo_parts_tower";
    tower_part.origin = {parts.origin.x + 4.0, parts.origin.y + 4.0, 10.8};
    tower_part.footprint = rectangle(10.0, 10.0);
    tower_part.levels_above = 4;
    tower_part.floor_heights.assign(4, 3.4);
    tower_part.roof.shape = "onion";
    parts.parts.push_back(std::move(tower_part));
    result.push_back(std::move(parts));
    require_unique_demo_geometry(result);
    return result;
}

std::vector<building_spec>
read_specs(const std::string &path, const std::string &fallback_id)
{
    json root;
    try {
	root = json::parse(read_text_file(path));
    } catch (const json::exception &error) {
	throw std::runtime_error(std::string("unable to parse JSON input: ") + error.what());
    }
    if (!root.is_object())
	throw std::runtime_error("input root must be a JSON object");
    std::vector<building_spec> result;
    const std::string root_type = root.contains("type") && root.at("type").is_string() ? root.at("type").get<std::string>() : "";
    if (root_type == "Feature" || root_type == "FeatureCollection")
	result = parse_geojson(root, fallback_id);
    else if (root_type == "CityJSON" || root_type == "CityJSONFeature")
	result = parse_cityjson(root);
    else if (root.contains("elements"))
	result = parse_overpass(root, fallback_id);
    else
	result.push_back(parse_native_object(root, fallback_id, "m", false));
    for (building_spec &spec : result)
	validate_spec(spec);
    return result;
}

void
validate_spec(const building_spec &spec)
{
    auto require_positive = [&spec](double value, const char *name) {
	if (!std::isfinite(value) || value <= 0.0)
	    throw std::runtime_error(spec.id + ": " + name + " must be positive and finite");
    };
    if (spec.id.empty())
	throw std::runtime_error("building id must not be empty");
    if (!std::isfinite(spec.origin.x) || !std::isfinite(spec.origin.y) || !std::isfinite(spec.origin.z))
	throw std::runtime_error(spec.id + ": origin coordinates must be finite");
    if (spec.footprint.size() < 3 || spec.footprint.size() > MAX_FOOTPRINT_POINTS)
	throw std::runtime_error(spec.id + ": footprint must contain between 3 and " + std::to_string(MAX_FOOTPRINT_POINTS) + " points");
    if (signed_area(spec.footprint) <= 1.0e-6)
	throw std::runtime_error(spec.id + ": footprint must be counter-clockwise and have positive area");
    double shortest_edge = std::numeric_limits<double>::infinity();
    for (size_t i = 0; i < spec.footprint.size(); ++i) {
	const point2 &a = spec.footprint[i];
	const point2 &b = spec.footprint[(i + 1) % spec.footprint.size()];
	if (!std::isfinite(a.x) || !std::isfinite(a.y))
	    throw std::runtime_error(spec.id + ": footprint contains a non-finite coordinate");
	shortest_edge = std::min(shortest_edge, std::hypot(b.x - a.x, b.y - a.y));
	for (size_t j = i + 1; j < spec.footprint.size(); ++j) {
	    const size_t i_next = (i + 1) % spec.footprint.size();
	    const size_t j_next = (j + 1) % spec.footprint.size();
	    if (i == j || i_next == j || j_next == i)
		continue;
	    if (segments_intersect(a, b, spec.footprint[j], spec.footprint[j_next]))
		throw std::runtime_error(spec.id + ": footprint self-intersects");
	}
    }
    require_positive(shortest_edge, "footprint edge length");
    if (spec.levels_above < 0 || spec.levels_above > MAX_LEVELS || spec.levels_below < 0 || spec.levels_below > MAX_LEVELS)
	throw std::runtime_error(spec.id + ": above/below-ground levels must be in [0, 200]");
    if (spec.min_level < 0 || spec.min_level > spec.levels_above)
	throw std::runtime_error(spec.id + ": min_level must be between zero and levels_above");
    if (spec.floor_heights.size() != static_cast<size_t>(spec.levels_above))
	throw std::runtime_error(spec.id + ": floor_heights count must equal levels_above");
    if (spec.basement_heights.size() != static_cast<size_t>(spec.levels_below))
	throw std::runtime_error(spec.id + ": basement_heights count must equal levels_below");
    for (double height : spec.floor_heights)
	require_positive(height, "floor height");
    for (double height : spec.basement_heights)
	require_positive(height, "basement height");
    require_positive(spec.wall_thickness, "wall_thickness");
    require_positive(spec.slab_thickness, "slab_thickness");
    require_positive(spec.foundation_thickness, "foundation_thickness");
    require_positive(spec.roof.thickness, "roof thickness");
    if (spec.wall_thickness >= shortest_edge * 0.45)
	throw std::runtime_error(spec.id + ": wall_thickness is too large for the shortest footprint edge");
    if (spec.roof.height < 0.0 || !std::isfinite(spec.roof.height))
	throw std::runtime_error(spec.id + ": roof height must be non-negative and finite");
    if (spec.roof.overhang < 0.0 || !std::isfinite(spec.roof.overhang))
	throw std::runtime_error(spec.id + ": roof overhang must be non-negative and finite");
    if (!std::isfinite(spec.roof.angle_degrees) ||
	(spec.roof.angle_degrees < 0.0 && std::fabs(spec.roof.angle_degrees + 1.0) > HEIGHT_CONSISTENCY_EPSILON_METERS) ||
	std::fabs(spec.roof.angle_degrees) <= HEIGHT_CONSISTENCY_EPSILON_METERS || spec.roof.angle_degrees > 89.0)
	throw std::runtime_error(spec.id + ": roof angle must be in (0, 89] degrees when specified");
    if (!std::isfinite(spec.roof.direction_degrees) ||
	(spec.roof.direction_degrees < 0.0 && std::fabs(spec.roof.direction_degrees + 1.0) > HEIGHT_CONSISTENCY_EPSILON_METERS) ||
	spec.roof.direction_degrees >= 360.0)
	throw std::runtime_error(spec.id + ": roof direction must be in [0, 360) degrees when specified");
    if (spec.roof.levels < 0 || spec.roof.levels > MAX_LEVELS)
	throw std::runtime_error(spec.id + ": roof levels must be in [0, 200]");
    if (spec.roof.orientation != "along" && spec.roof.orientation != "across")
	throw std::runtime_error(spec.id + ": roof orientation must be 'along' or 'across'");
    if (roof_shapes.count(normalized_token(spec.roof.shape)) == 0 && spec.roof.shape.empty())
	throw std::runtime_error(spec.id + ": roof shape must not be empty");
    const double roof_height = effective_roof_height(spec);
    if (spec.total_height >= 0.0 && spec.total_height <= roof_height)
	throw std::runtime_error(spec.id + ": total height must exceed roof height");
    if (spec.total_height > 0.0) {
	double storey_height = 0.0;
	for (double height : spec.floor_heights)
	    storey_height += height;
	if (storey_height > spec.total_height - roof_height + HEIGHT_CONSISTENCY_EPSILON_METERS)
	    throw std::runtime_error(spec.id + ": floor heights exceed the facade height available below the roof");
    }
    for (size_t wall : spec.wall_indices) {
	if (wall >= spec.footprint.size())
	    throw std::runtime_error(spec.id + ": wall_indices contains an out-of-range wall");
    }
    for (const opening_spec &opening : spec.openings) {
	if (opening.kind != "door" && opening.kind != "window" && opening.kind != "opening")
	    throw std::runtime_error(spec.id + ": opening kind must be door, window, or opening");
	if (opening.wall >= spec.footprint.size())
	    throw std::runtime_error(spec.id + ": opening references an out-of-range wall");
	if (opening.level < -spec.levels_below || opening.level >= spec.levels_above)
	    throw std::runtime_error(spec.id + ": opening level is outside the building's storeys");
	require_positive(opening.width, "opening width");
	require_positive(opening.height, "opening height");
	require_positive(opening.frame_width, "opening frame_width");
	require_positive(opening.frame_depth, "opening frame_depth");
	if (opening.count < 1 || opening.count > 1000)
	    throw std::runtime_error(spec.id + ": opening count must be in [1, 1000]");
	if (!std::isfinite(opening.offset) || !std::isfinite(opening.sill) || !std::isfinite(opening.spacing) ||
	    opening.offset < 0.0 || opening.sill < 0.0 || opening.spacing < 0.0)
	    throw std::runtime_error(spec.id + ": opening offset, sill, and spacing must be finite and non-negative");
	const point2 &a = spec.footprint[opening.wall];
	const point2 &b = spec.footprint[(opening.wall + 1) % spec.footprint.size()];
	const double wall_length = std::hypot(b.x - a.x, b.y - a.y);
	const double last_end = opening.offset + static_cast<double>(opening.count - 1) * opening.spacing + opening.width;
	if (opening.offset < 0.0 || last_end > wall_length)
	    throw std::runtime_error(spec.id + ": opening extent exceeds its wall");
	const double storey_height = opening.level >= 0 ? spec.floor_heights[static_cast<size_t>(opening.level)] : spec.basement_heights[static_cast<size_t>(-opening.level - 1)];
	if (opening.sill < 0.0 || opening.sill + opening.height > storey_height)
	    throw std::runtime_error(spec.id + ": opening vertical extent exceeds its storey");
	(void)level_base(spec, opening.level);
    }
    if (spec.structure.enabled) {
	require_positive(spec.structure.grid_x, "structure grid_x");
	require_positive(spec.structure.grid_y, "structure grid_y");
	require_positive(spec.structure.column_width, "structure column_width");
	require_positive(spec.structure.column_depth, "structure column_depth");
	require_positive(spec.structure.column_diameter, "structure column_diameter");
	require_positive(spec.structure.beam_width, "structure beam_width");
	require_positive(spec.structure.beam_depth, "structure beam_depth");
	if (spec.structure.column_shape != "rectangular" && spec.structure.column_shape != "round")
	    throw std::runtime_error(spec.id + ": column_shape must be rectangular or round");
    }
    for (const installation_spec &installation : spec.installations) {
	if (!std::isfinite(installation.position.x) || !std::isfinite(installation.position.y) ||
	    !std::isfinite(installation.position.z))
	    throw std::runtime_error(spec.id + ": installation position coordinates must be finite");
	require_positive(installation.size.x, "installation size x");
	require_positive(installation.size.y, "installation size y");
	require_positive(installation.size.z, "installation size z");
	if (installation.shape != "box" && installation.shape != "cylinder")
	    throw std::runtime_error(spec.id + ": installation shape must be box or cylinder");
    }
    for (const building_spec &part : spec.parts)
	validate_spec(part);
}

std::string
effective_spec_json(const std::vector<building_spec> &specs)
{
    if (specs.size() == 1)
	return spec_to_json(specs.front()).dump(2) + "\n";
    json result = {{"version", 1}, {"buildings", json::array()}};
    for (const building_spec &spec : specs)
	result["buildings"].push_back(spec_to_json(spec));
    return result.dump(2) + "\n";
}

std::string
schema_json()
{
    return building_schema_json;
}

std::string
supported_types_text()
{
    return
	"OSM building taxonomy (documented values; user-defined values are preserved):\n"
	"  accommodation: apartments annexe barracks bungalow cabin detached dormitory farm ger hotel house houseboat residential semidetached_house static_caravan stilt_house terrace tree_house trullo\n"
	"  commercial: commercial industrial kiosk office retail supermarket warehouse\n"
	"  religious: religious cathedral chapel church kingdom_hall monastery mosque presbytery shrine synagogue temple\n"
	"  civic: bakehouse bridge civic clock_tower college fire_station government gatehouse hospital kindergarten museum public school toilets train_station transportation university\n"
	"  agricultural: barn conservatory cowshed farm_auxiliary greenhouse slurry_tank stable sty livestock\n"
	"  sports: grandstand pavilion riding_hall sports_hall sports_centre stadium\n"
	"  storage/vehicles: allotment_house boathouse hangar hut shed carport garage garages parking\n"
	"  technical: digester service tech_cab transformer_tower water_tower storage_tank silo\n"
	"  other: beach_hut bunker castle construction container guardhouse military outbuilding pagoda quonset_hut roof ruins ship tent tower triumphal_arch windmill yes\n";
}

std::string
supported_roofs_text()
{
    std::ostringstream out;
    out << "OSM roof:shape values:\n  ";
    size_t count = 0;
    for (const std::string &shape : roof_shapes) {
	if (count++ > 0)
	    out << ' ';
	out << shape;
    }
    out << "\nUnknown user-defined values are retained as metadata and realized as flat roofs.\n";
    return out.str();
}

} // namespace building

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
