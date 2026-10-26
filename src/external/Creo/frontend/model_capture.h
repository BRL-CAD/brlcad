/**
 *           M O D E L _ C A P T U R E . H
 * BRL-CAD
 */

#ifndef CREO_FRONTEND_MODEL_CAPTURE_H
#define CREO_FRONTEND_MODEL_CAPTURE_H

#include <stddef.h>

#include <string>
#include <vector>

#include "creo-brl.h"
#include "conversion_snapshot.h"


struct snapshot_settings_strings {
    std::string output_path;
    std::string log_path;
    std::string material_path;
    std::string rename_parameters;
    std::string preserve_parameters;
};


struct snapshot_part_strings {
    std::string model_name;
    std::string model_version;
    std::string material_name;
};


struct snapshot_named_value_capture {
    std::string name;
    std::string value;
};


struct snapshot_part_capture {
    struct creo_brl_snapshot_part part;
    struct snapshot_part_strings strings;
    std::vector<struct snapshot_named_value_capture> attributes;
    std::vector<struct snapshot_named_value_capture> parameters;
    std::vector<struct creo_brl_snapshot_tgc_subtraction> tgc_subtractions;
    std::vector<struct creo_brl_snapshot_vector> vertices;
    std::vector<struct creo_brl_snapshot_triangle> triangles;
    std::vector<struct creo_brl_snapshot_vector> normals;
    std::vector<uint32_t> normal_indices;
};


struct snapshot_mesh_capture {
    std::vector<struct creo_brl_snapshot_vector> vertices;
    std::vector<struct creo_brl_snapshot_triangle> triangles;
    std::vector<struct creo_brl_snapshot_vector> normals;
    std::vector<uint32_t> normal_indices;
    int is_solid;
};


struct snapshot_tessellation_attempt {
    double chord;
    double angle;
};


struct snapshot_tessellation_policy {
    std::vector<struct snapshot_tessellation_attempt> attempts;
    size_t maximum_facet_count;
};


typedef int (*snapshot_tessellation_attempt_fn)(
    void *context,
    const struct snapshot_tessellation_attempt *attempt,
    size_t maximum_facet_count,
    struct snapshot_mesh_capture *mesh);


struct snapshot_assembly_capture {
    struct creo_brl_snapshot_assembly assembly;
    std::string model_name;
    std::string model_version;
    std::vector<struct creo_brl_snapshot_assembly_member> members;
};


struct snapshot_scene_capture {
    struct creo_brl_snapshot_settings settings;
    struct snapshot_settings_strings settings_strings;
    std::vector<struct snapshot_part_capture> parts;
    std::vector<struct snapshot_assembly_capture> assemblies;
};


int creo_brl_build_tessellation_policy(
    const struct creo_brl_snapshot_settings *settings,
    double model_to_mm,
    double bbox_diagonal_mm,
    struct snapshot_tessellation_policy *policy);
int creo_brl_capture_tessellation_data(
    const ProSurfaceTessellationData *tessellation,
    int surface_count,
    double model_to_mm,
    int write_normals,
    size_t maximum_facet_count,
    struct snapshot_mesh_capture *mesh);
int creo_brl_run_tessellation_policy(
    const struct snapshot_tessellation_policy *policy,
    int require_solidity,
    snapshot_tessellation_attempt_fn capture_attempt,
    void *context,
    struct snapshot_mesh_capture *mesh);
void creo_brl_mark_bbox_fallback(struct snapshot_part_capture *capture);


int creo_brl_frontend_capture_model_scene(ProMdl model,
                                          ProMdlType model_type,
                                          const struct creo_brl_snapshot_settings *settings,
                                          const struct snapshot_settings_strings *settings_strings,
                                          struct snapshot_scene_capture *capture,
                                          uint32_t *root_type,
                                          uint32_t *root_index);


#endif /* CREO_FRONTEND_MODEL_CAPTURE_H */
