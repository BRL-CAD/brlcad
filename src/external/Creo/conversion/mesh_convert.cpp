/**
 *         M E S H _ C O N V E R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/**
 * @file conversion/mesh_convert.cpp
 *
 * Toolkit-free mesh and fallback output helpers for snapshot part conversion.
 */

#include "common.h"

#include "conversion/mesh_convert.h"

#include <string.h>

#include "bu/avs.h"
#include "part_writer.h"
#include "shared/core_api.h"
#include "wdb.h"


static const char CREO_BRL_CORE_SOLID_SUFFIX[] = ".s";
static const char CREO_BRL_CORE_RPP_SUFFIX[] = ".rpp";
static const char CREO_BRL_CORE_REGION_SUFFIX[] = ".r";
static const char CREO_BRL_CORE_UNKNOWN_NAME[] = "unknown";
static const char CREO_BRL_CORE_NAME_KEEP_CHARACTERS[] = "+-.=_";
const char CREO_BRL_CORE_IMPORTER_ATTRIBUTE[] = "importer";
const char CREO_BRL_CORE_IMPORTER_VALUE[] = "creo-g";
const char CREO_BRL_CORE_MODEL_NAME_ATTRIBUTE[] = "ptc_name";
const char CREO_BRL_CORE_MODEL_VERSION_ATTRIBUTE[] = "ptc_version_stamp";
const char CREO_BRL_CORE_MATERIAL_ATTRIBUTE[] = "ptc_material_name";
const char CREO_BRL_CORE_FALLBACK_ATTRIBUTE[] = "tess_fail";
static const char CREO_BRL_CORE_UNDEFINED_MATERIAL[] = "undefined";


std::string
creo_brl_snapshot_name_root(const std::string& model_name)
{
    std::string name;
    size_t index = 0;

    name.reserve(model_name.size());
    for (index = 0; index < model_name.size(); ++index) {
        const unsigned char character = (unsigned char)model_name[index];
        const int is_uppercase = character >= 'A' && character <= 'Z';
        const int is_lowercase = character >= 'a' && character <= 'z';
        const int is_digit = character >= '0' && character <= '9';

        if (is_uppercase)
            name.push_back((char)(character - 'A' + 'a'));
        else if (is_lowercase || is_digit ||
                 strchr(CREO_BRL_CORE_NAME_KEEP_CHARACTERS, character))
            name.push_back((char)character);
        else if (name.empty() || name[name.size() - 1] != '_')
            name.push_back('_');
    }

    if (name.empty())
        name.assign(CREO_BRL_CORE_UNKNOWN_NAME);

    return name;
}


static int
snapshot_object_names(struct db_i *database,
                      const std::string& name_root,
                      const char *solid_suffix,
                      std::string *solid_name,
                      std::string *region_name)
{
    uint32_t suffix = 0;

    for (;;) {
        std::string object_root = name_root;

        if (suffix > 0)
            object_root += "_" + std::to_string(suffix);

        *solid_name = object_root + solid_suffix;
        *region_name = object_root + CREO_BRL_CORE_REGION_SUFFIX;
        if (db_lookup(database, solid_name->c_str(), LOOKUP_QUIET) == RT_DIR_NULL &&
            db_lookup(database, region_name->c_str(), LOOKUP_QUIET) == RT_DIR_NULL)
            return 1;

        if (suffix == UINT32_MAX)
            return 0;
        ++suffix;
    }
}


static int
snapshot_write_attributes(struct db_i *database,
                          const char *solid_name,
                          const char *region_name,
                          const std::string& model_name,
                          const std::string& model_version,
                          const std::string& material_name,
                          int has_mesh)
{
    struct bu_attribute_value_set attributes = BU_AVS_INIT_ZERO;
    struct directory *solid = db_lookup(database, solid_name, LOOKUP_QUIET);
    struct directory *region = db_lookup(database, region_name, LOOKUP_QUIET);
    int result = 0;

    if (solid == RT_DIR_NULL || region == RT_DIR_NULL ||
        db5_get_attributes(database, &attributes, solid) != 0)
        goto cleanup;

    bu_avs_add(&attributes, CREO_BRL_CORE_IMPORTER_ATTRIBUTE, CREO_BRL_CORE_IMPORTER_VALUE);
    bu_avs_add(&attributes, CREO_BRL_CORE_MODEL_NAME_ATTRIBUTE, model_name.c_str());
    if (!has_mesh)
        bu_avs_add(&attributes, CREO_BRL_CORE_FALLBACK_ATTRIBUTE, model_name.c_str());
    if (db5_update_attributes(solid, &attributes, database) != 0)
        goto cleanup;

    bu_avs_free(&attributes);
    if (db5_get_attributes(database, &attributes, region) != 0)
        goto cleanup;

    bu_avs_add(&attributes, CREO_BRL_CORE_MODEL_NAME_ATTRIBUTE, model_name.c_str());
    if (!model_version.empty())
        bu_avs_add(&attributes, CREO_BRL_CORE_MODEL_VERSION_ATTRIBUTE, model_version.c_str());
    bu_avs_add(&attributes, CREO_BRL_CORE_MATERIAL_ATTRIBUTE,
               material_name.empty() ? CREO_BRL_CORE_UNDEFINED_MATERIAL : material_name.c_str());
    result = db5_update_attributes(region, &attributes, database) == 0;

cleanup:
    bu_avs_free(&attributes);
    return result;
}


int
creo_brl_convert_snapshot_part(const struct creo_brl_snapshot_part *part,
                               const std::string& model_name,
                               const std::string& model_version,
                               const std::string& material_name,
                               std::vector<fastf_t>& vertices,
                               std::vector<int>& triangles,
                               std::vector<fastf_t>& normals,
                               std::vector<int>& normal_indices,
                               struct db_i *database,
                               struct rt_wdb *writer,
                               struct snapshot_part_output *output)
{
    const int has_mesh = (part->flags & CREO_BRL_SNAPSHOT_PART_HAS_MESH) != 0;
    const int has_normals = (part->flags & CREO_BRL_SNAPSHOT_PART_HAS_NORMALS) != 0;
    std::string solid_name;
    std::string region_name;
    struct bu_list members;

    if (!part || !database || !writer || !output)
        return CREO_BRL_CORE_CONVERT_INVALID_REQUEST;

    if (!snapshot_object_names(database, creo_brl_snapshot_name_root(model_name),
                               has_mesh ? CREO_BRL_CORE_SOLID_SUFFIX : CREO_BRL_CORE_RPP_SUFFIX,
                               &solid_name, &region_name) ||
        (has_mesh ?
         creo_brl_write_bot(writer, solid_name.c_str(), has_normals,
                            (size_t)part->vertex_count, (size_t)part->triangle_count,
                            vertices.data(), triangles.data(),
                            has_normals ? (size_t)part->normal_count : 0,
                            has_normals ? normals.data() : NULL,
                            has_normals ? normal_indices.data() : NULL) :
         mk_rpp(writer, solid_name.c_str(), part->bbox_min.coordinates,
                part->bbox_max.coordinates)) != 0)
        return CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;

    BU_LIST_INIT(&members);
    if (mk_addmember(solid_name.c_str(), &members, NULL, WMOP_UNION) == WMEMBER_NULL) {
        mk_freemembers(&members);
        return CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;
    }

    if (mk_comb(writer, region_name.c_str(), &members, 1, NULL, NULL,
                part->color_is_set ? part->color : NULL,
                (int)part->region_id, 0, 0, 0, 0, 0, 0) != 0 ||
        !snapshot_write_attributes(database, solid_name.c_str(), region_name.c_str(),
                                   model_name, model_version, material_name, has_mesh))
        return CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;

    output->region_name = region_name;
    return CREO_BRL_CORE_CONVERT_SUCCESS;
}
