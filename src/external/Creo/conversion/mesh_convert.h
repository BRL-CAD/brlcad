/**
 *         M E S H _ C O N V E R T . H
 * BRL-CAD
 */

#ifndef CREO_CONVERSION_MESH_CONVERT_H
#define CREO_CONVERSION_MESH_CONVERT_H

#include <stdio.h>

#include <string>

#include "shared/conversion_snapshot.h"


struct db_i;
struct rt_wdb;


struct snapshot_part_output {
    std::string region_name;
    int used_bbox_fallback;
};


struct snapshot_material_translation {
    int material_id;
    int los;
};


int creo_brl_write_snapshot_assembly_attributes(
    struct db_i *database,
    const char *assembly_name,
    const struct snapshot_assembly_capture& assembly_capture);

int creo_brl_convert_snapshot_part(
    const struct snapshot_part_capture& part_capture,
    const struct snapshot_scene_capture& scene,
    const struct snapshot_material_translation *material_translation,
    FILE *log_file,
    struct db_i *database,
    struct rt_wdb *writer,
    struct snapshot_part_output *output);


#endif /* CREO_CONVERSION_MESH_CONVERT_H */
