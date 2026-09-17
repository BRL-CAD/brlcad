/**
 *         M E S H _ C O N V E R T . H
 * BRL-CAD
 */

#ifndef CREO_CONVERSION_MESH_CONVERT_H
#define CREO_CONVERSION_MESH_CONVERT_H

#include <stdio.h>

#include <string>
#include <vector>
#include <vmath.h>

#include "common.h"
#include "conversion_snapshot.h"


struct db_i;
struct rt_wdb;

extern const char CREO_BRL_CORE_IMPORTER_ATTRIBUTE[];
extern const char CREO_BRL_CORE_IMPORTER_VALUE[];
extern const char CREO_BRL_CORE_MODEL_NAME_ATTRIBUTE[];
extern const char CREO_BRL_CORE_MODEL_VERSION_ATTRIBUTE[];
extern const char CREO_BRL_CORE_MATERIAL_ATTRIBUTE[];
extern const char CREO_BRL_CORE_FALLBACK_ATTRIBUTE[];


struct snapshot_part_output {
    std::string region_name;
    int used_bbox_fallback;
};


struct snapshot_named_value {
    std::string name;
    std::string value;
};


int creo_brl_convert_snapshot_part(const struct creo_brl_snapshot_part *part,
                                   const struct creo_brl_snapshot_settings *settings,
                                   const std::string& rename_parameters,
                                   const std::string& model_name,
                                   const std::string& model_version,
                                   const std::string& material_name,
                                   const std::vector<struct snapshot_named_value>& attributes,
                                   const std::vector<struct snapshot_named_value>& parameters,
                                   const std::vector<std::string>& subtraction_names,
                                   std::vector<fastf_t>& vertices,
                                   std::vector<int>& triangles,
                                   std::vector<fastf_t>& normals,
                                   std::vector<int>& normal_indices,
                                   FILE *log_file,
                                   struct db_i *database,
                                   struct rt_wdb *writer,
                                   struct snapshot_part_output *output);


#endif /* CREO_CONVERSION_MESH_CONVERT_H */
