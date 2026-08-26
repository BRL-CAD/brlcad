/**
 *            P R O F I L E _ C O N T R O L S . H
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
 */

#ifndef CREO_PROFILE_CONTROLS_H
#define CREO_PROFILE_CONTROLS_H

#define CREO_PROFILE_RADIO_BUTTON_MAX 4

static const char creo_profile_file_name[] = "Creo_to_BRL.g";
static const char creo_profile_material_resource[] = "mtl_fname";
static const char creo_profile_legacy_material_file_path[] = "./ptc_materials.mtl";
static const char creo_profile_bundle_material_file_name[] = "ptc_materials.mtl";

struct creo_profile_input {
    const char *value;
    int index;
};

struct creo_profile_control {
    const char *attribute;
    const char *resource;
    const char *type;
    int input_indices[CREO_PROFILE_RADIO_BUTTON_MAX];
    int index;
};

static const struct creo_profile_input creo_profile_inputs[] = {
    {"./ptc_materials.mtl", 1},
    {"0.0", 2},
    {"0.3", 3},
    {"0.5", 4},
    {"1.0", 5},
    {"1000", 6},
    {"30", 7},
    {"all/(debug)", 8},
    {"failure", 9},
    {"failure/success", 10},
    {"millimeter", 11},
    {"nomenclature", 12},
    {"nomenclature,part_number,ptc_material_name", 13},
    {"none", 14},
    {"off", 15},
    {"on", 16},
    {"percent", 17},
    {"success", 18},
    {"x_to_z", 19},
    {"y_to_z", 20},
    {NULL, -1}
};

static const struct creo_profile_control creo_profile_controls[] = {
    {"process_log_criteria", "log_file_type", "RAD", {8, 9, 18, 10}, 1},
    {"material_file_name", "mtl_fname", "STR", {1, -1, -1, -1}, 2},
    {"create_object_names", "param_rename", "STR", {12, -1, -1, -1}, 3},
    {"preserved_attributes", "param_save", "STR", {13, -1, -1, -1}, 4},
    {"coordinate_transformation", "transform", "RAD", {14, 19, 20, -1}, 5},
    {"initial_region_counter", "region_counter", "STR", {6, -1, -1, -1}, 6},
    {"minimum_luminance", "min_luminance", "STR", {7, -1, -1, -1}, 7},
    {"chord_mode", "chord_mode", "RAD", {17, 11, -1, -1}, 8},
    {"maximum_chord_height", "max_chord", "STR", {3, -1, -1, -1}, 9},
    {"minimum_angle_control", "min_angle", "STR", {4, -1, -1, -1}, 10},
    {"eliminate_small_features", "elim_small", "BOX", {15, -1, -1, -1}, 11},
    {"minimum_hole_diameter", "min_hole", "STR", {2, -1, -1, -1}, 12},
    {"minimum_chamfer_dimension", "min_chamfer", "STR", {2, -1, -1, -1}, 13},
    {"minimum_blend_radius", "min_round", "STR", {2, -1, -1, -1}, 14},
    {"facetize_everything", "facets_only", "BOX", {16, -1, -1, -1}, 15},
    {"export_facets_to_stl", "export_stl", "BOX", {15, -1, -1, -1}, 16},
    {"reject_failed_bots", "check_solidity", "BOX", {15, -1, -1, -1}, 17},
    {"box_replaces_failed_part", "create_boxes", "BOX", {15, -1, -1, -1}, 18},
    {"write_surface_normals", "write_normals", "BOX", {15, -1, -1, -1}, 19},
    {NULL, NULL, NULL, {-1, -1, -1, -1}, -1}
};

#endif /* CREO_PROFILE_CONTROLS_H */
