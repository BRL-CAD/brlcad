/**
 *         M E S H _ C O N V E R T . H
 * BRL-CAD
 */

#ifndef CREO_CONVERSION_MESH_CONVERT_H
#define CREO_CONVERSION_MESH_CONVERT_H

#include <string>
#include <vector>

#include "common.h"
#include "vmath.h"
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
};


std::string creo_brl_snapshot_name_root(const std::string& model_name);

int creo_brl_convert_snapshot_part(const struct creo_brl_snapshot_part *part,
                                   const std::string& model_name,
                                   const std::string& model_version,
                                   const std::string& material_name,
                                   std::vector<fastf_t>& vertices,
                                   std::vector<int>& triangles,
                                   std::vector<fastf_t>& normals,
                                   std::vector<int>& normal_indices,
                                   struct db_i *database,
                                   struct rt_wdb *writer,
                                   struct snapshot_part_output *output);


#endif /* CREO_CONVERSION_MESH_CONVERT_H */
