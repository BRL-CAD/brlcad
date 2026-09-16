/*                      B U I L D I N G . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef SHAPES_BUILDING_BUILDING_H
#define SHAPES_BUILDING_BUILDING_H

#include "common.h"

#include <map>
#include <string>
#include <vector>

namespace building {

struct point2 {
    double x = 0.0;
    double y = 0.0;
};

struct point3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct rgb_color {
    unsigned char r = 190;
    unsigned char g = 185;
    unsigned char b = 175;
};

struct roof_spec {
    std::string shape = "flat";
    double height = 0.35;
    double angle_degrees = -1.0;
    int levels = 0;
    std::string orientation = "along";
    double direction_degrees = -1.0;
    double overhang = 0.25;
    double thickness = 0.18;
    std::string material = "roof_tiles";
    rgb_color color = {105, 72, 55};
};

struct opening_spec {
    std::string id;
    std::string kind = "window";
    std::string type = "yes";
    size_t wall = 0;
    int level = 0;
    double offset = 1.0;
    double width = 1.2;
    double height = 1.2;
    double sill = 0.9;
    int count = 1;
    double spacing = 2.4;
    double frame_width = 0.075;
    double frame_depth = 0.08;
    std::string material;
    rgb_color color = {105, 80, 55};
    std::map<std::string, std::string> osm_tags;
};

struct structure_spec {
    std::string system = "wall_bearing";
    bool enabled = false;
    double grid_x = 6.0;
    double grid_y = 6.0;
    std::string column_shape = "rectangular";
    double column_width = 0.30;
    double column_depth = 0.30;
    double column_diameter = 0.35;
    double beam_width = 0.25;
    double beam_depth = 0.45;
    std::string material = "reinforced_concrete";
    rgb_color color = {145, 145, 140};
};

struct installation_spec {
    std::string id;
    std::string kind = "generic";
    std::string shape = "box";
    point3 position;
    point3 size = {1.0, 1.0, 1.0};
    std::string material = "metal";
    rgb_color color = {125, 125, 125};
    std::map<std::string, std::string> attributes;
};

struct building_spec {
    int version = 1;
    std::string id = "building";
    std::string type = "yes";
    std::string building_class;
    std::vector<std::string> function;
    std::vector<std::string> usage;
    point3 origin;
    std::vector<point2> footprint;

    int levels_above = 1;
    int levels_below = 0;
    int min_level = 0;
    std::vector<double> floor_heights = {3.0};
    std::vector<double> basement_heights;
    std::vector<int> non_existent_levels;
    double total_height = -1.0;
    double min_height = -1.0;

    bool walls = true;
    std::vector<size_t> wall_indices;
    double wall_thickness = 0.20;
    double slab_thickness = 0.20;
    double foundation_thickness = 0.30;
    std::string wall_material = "concrete";
    rgb_color wall_color;
    roof_spec roof;

    bool auto_openings = true;
    std::vector<opening_spec> openings;
    structure_spec structure;
    std::vector<installation_spec> installations;
    std::vector<building_spec> parts;

    std::map<std::string, std::string> osm_tags;
    std::map<std::string, std::string> cityjson_attributes;
    std::map<std::string, std::string> metadata;
};

struct generation_report {
    size_t building_count = 0;
    size_t part_count = 0;
    size_t region_count = 0;
    size_t primitive_count = 0;
    size_t opening_count = 0;
    size_t column_count = 0;
    size_t beam_count = 0;
};

building_spec make_preset(const std::string &preset);
std::vector<building_spec> make_demo_specs();
double effective_roof_height(const building_spec &spec);
std::vector<building_spec> read_specs(const std::string &path, const std::string &fallback_id);
void validate_spec(const building_spec &spec);
std::string effective_spec_json(const std::vector<building_spec> &specs);
std::string schema_json();
std::string supported_types_text();
std::string supported_roofs_text();

generation_report write_database(
    const std::vector<building_spec> &specs,
    const std::string &output_path,
    bool force,
    bool append);

} // namespace building

#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
