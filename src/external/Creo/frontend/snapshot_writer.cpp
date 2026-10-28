/**
 *            S N A P S H O T _ W R I T E R . C P P
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
 * @file snapshot_writer.cpp
 *
 * Creo Toolkit capture for neutral conversion requests.
 */

#include "common.h"

#include <stdio.h>
#include <string>
#include <vector>
#include <windows.h>

#include "frontend/creo_toolkit.h"
#include "shared/conversion_snapshot.h"
#include "frontend/model_capture.h"
#include "frontend/profile_ui.h"
#include "frontend/snapshot_writer.h"


static_assert(sizeof(struct creo_brl_snapshot_header) == CREO_BRL_SNAPSHOT_HEADER_SIZE,
              "Snapshot header layout changed");
static_assert(sizeof(struct creo_brl_snapshot_settings) == CREO_BRL_SNAPSHOT_SETTINGS_SIZE,
              "Snapshot settings layout changed");
static_assert(sizeof(struct creo_brl_snapshot_tgc_subtraction) == CREO_BRL_SNAPSHOT_TGC_SUBTRACTION_SIZE,
              "Snapshot TGC subtraction layout changed");
static_assert(sizeof(struct creo_brl_snapshot_part) == CREO_BRL_SNAPSHOT_PART_SIZE,
              "Snapshot part layout changed");
static_assert(sizeof(struct creo_brl_snapshot_assembly_member) ==
                  CREO_BRL_SNAPSHOT_ASSEMBLY_MEMBER_SIZE,
              "Snapshot assembly member layout changed");
static_assert(sizeof(struct creo_brl_snapshot_assembly) == CREO_BRL_SNAPSHOT_ASSEMBLY_SIZE,
              "Snapshot assembly layout changed");
static_assert(sizeof(struct creo_brl_snapshot_scene) == CREO_BRL_SNAPSHOT_SCENE_SIZE,
              "Snapshot scene layout changed");


static int
dialog_text(const char *name, std::string *text)
{
    wchar_t *wide_text = NULL;
    std::string converted_text;
    int size = 0;
    const ProError err = ProUIInputpanelValueGet(CREO_UI_NAME, (char *)name, &wide_text);
    int converted = 0;

    if (err == PRO_TK_NO_ERROR && wide_text && text) {
        size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_text, -1,
                                   NULL, 0, NULL, NULL);
        if (size > 0) {
            converted_text.resize((size_t)size);
            converted = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_text, -1,
                                             &converted_text[0], size, NULL, NULL) == size;
            if (converted) {
                converted_text.resize((size_t)size - 1);
                *text = converted_text;
            }
        }
    }

    if (wide_text)
        (void)ProWstringFree(wide_text);

    return converted;
}


static int
dialog_get_check(void *UNUSED(context), const char *name, int *checked)
{
    ProBoolean state = PRO_B_FALSE;

    if (!checked || ProUICheckbuttonGetState(CREO_UI_NAME, (char *)name, &state) != PRO_TK_NO_ERROR)
        return 0;

    *checked = state != PRO_B_FALSE;
    return 1;
}


static int
dialog_get_radio(void *UNUSED(context), const char *name, std::string *selection)
{
    char **names = NULL;
    int count = 0;
    const ProError err = ProUIRadiogroupSelectednamesGet(CREO_UI_NAME, (char *)name, &count, &names);

    if (err != PRO_TK_NO_ERROR || count != 1 || !names || !names[0]) {
        if (names)
            ProStringarrayFree(names, count);
        return 0;
    }

    *selection = names[0];
    ProStringarrayFree(names, count);
    return 1;
}


static int
dialog_get_text(void *UNUSED(context), const char *name, std::string *text)
{
    return dialog_text(name, text);
}


static const struct creo_brl_frontend_control_api snapshot_controls = {
    NULL,
    NULL,
    NULL,
    NULL,
    dialog_get_text,
    dialog_get_check,
    dialog_get_radio
};


static int
capture_settings(struct creo_brl_snapshot_settings *settings, snapshot_settings_strings *strings)
{
    struct creo_brl_frontend_snapshot_options captured = {};

    if (!settings || !strings ||
        !creo_brl_frontend_capture_snapshot_options(&snapshot_controls, &captured))
        return 0;

    *settings = captured.settings;
    strings->output_path = captured.output_path;
    strings->log_path = captured.log_path;
    strings->material_path = captured.material_path;
    strings->rename_parameters = captured.rename_parameters;
    strings->preserve_parameters = captured.preserve_parameters;
    return 1;
}


static int
snapshot_open_for_write(const char *snapshot_path, FILE **snapshot_file)
{
    const int wide_path_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                                     snapshot_path, -1, NULL, 0);
    std::vector<wchar_t> wide_path;

    if (wide_path_length <= 0)
        return 0;

    wide_path.resize((size_t)wide_path_length);
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, snapshot_path, -1,
                            &wide_path[0], wide_path_length) != wide_path_length)
        return 0;

    *snapshot_file = _wfopen(&wide_path[0], L"wb");
    return *snapshot_file != NULL;
}


static int
write_range(FILE *snapshot_file, const void *data, uint64_t size,
            struct creo_brl_snapshot_range *range)
{
    const __int64 offset = _ftelli64(snapshot_file);

    if (!range)
        return 0;

    if (size == 0) {
        range->offset = 0;
        range->size = 0;
        return 1;
    }

    if (offset < 0 || fwrite(data, 1, (size_t)size, snapshot_file) != size)
        return 0;

    range->offset = (uint64_t)offset;
    range->size = size;
    return 1;
}


static int
write_text(FILE *snapshot_file, const std::string& text, struct creo_brl_snapshot_range *range)
{
    return write_range(snapshot_file, text.data(), (uint64_t)text.size(), range);
}


static int
write_named_values(FILE *snapshot_file,
                   const std::vector<struct snapshot_named_value_capture>& values,
                   struct creo_brl_snapshot_range *range,
                   uint64_t *count)
{
    std::vector<struct creo_brl_snapshot_named_value> records(values.size());
    const __int64 records_offset = _ftelli64(snapshot_file);

    if (!range || !count || records_offset < 0)
        return 0;

    *count = values.size();
    if (values.empty()) {
        range->offset = 0;
        range->size = 0;
        return 1;
    }

    range->offset = (uint64_t)records_offset;
    range->size = values.size() * sizeof(records[0]);
    if (fwrite(records.data(), 1, (size_t)range->size, snapshot_file) != range->size)
        return 0;

    for (size_t index = 0; index < values.size(); ++index) {
        if (!write_text(snapshot_file, values[index].name, &records[index].name) ||
            !write_text(snapshot_file, values[index].value, &records[index].value))
            return 0;
    }

    if (_fseeki64(snapshot_file, records_offset, SEEK_SET) != 0 ||
        fwrite(records.data(), 1, (size_t)range->size, snapshot_file) != range->size)
        return 0;

    return _fseeki64(snapshot_file, 0, SEEK_END) == 0;
}


static int
write_settings_strings(FILE *snapshot_file,
                       struct creo_brl_snapshot_settings *settings,
                       const struct snapshot_settings_strings& strings)
{
    return write_text(snapshot_file, strings.output_path, &settings->output_path) &&
           write_text(snapshot_file, strings.log_path, &settings->log_path) &&
           write_text(snapshot_file, strings.material_path, &settings->material_path) &&
           write_text(snapshot_file, strings.rename_parameters, &settings->rename_parameters);
}


static int
write_part_capture(FILE *snapshot_file, struct snapshot_part_capture *capture)
{
    capture->part.vertex_count = capture->vertices.size();
    capture->part.triangle_count = capture->triangles.size();
    capture->part.normal_count = capture->normals.size();
    capture->part.normal_index_count = capture->normal_indices.size();
    capture->part.tgc_subtraction_count = capture->tgc_subtractions.size();
    capture->part.attribute_count = capture->attributes.size();
    capture->part.parameter_count = capture->parameters.size();

    return write_text(snapshot_file, capture->strings.model_name, &capture->part.model_name) &&
           write_text(snapshot_file, capture->strings.model_version, &capture->part.model_version) &&
           write_text(snapshot_file, capture->strings.material_name, &capture->part.material_name) &&
           write_range(snapshot_file, capture->vertices.data(),
                       capture->vertices.size() * sizeof(capture->vertices[0]),
                       &capture->part.vertices) &&
           write_range(snapshot_file, capture->triangles.data(),
                       capture->triangles.size() * sizeof(capture->triangles[0]),
                       &capture->part.triangles) &&
           write_range(snapshot_file, capture->normals.data(),
                       capture->normals.size() * sizeof(capture->normals[0]),
                       &capture->part.normals) &&
           write_range(snapshot_file, capture->normal_indices.data(),
                       capture->normal_indices.size() * sizeof(capture->normal_indices[0]),
                       &capture->part.normal_indices) &&
           write_range(snapshot_file, capture->tgc_subtractions.data(),
                       capture->tgc_subtractions.size() * sizeof(capture->tgc_subtractions[0]),
                       &capture->part.tgc_subtractions) &&
           write_named_values(snapshot_file, capture->attributes, &capture->part.attributes,
                              &capture->part.attribute_count) &&
           write_named_values(snapshot_file, capture->parameters, &capture->part.parameters,
                              &capture->part.parameter_count);
}


static int
finish_snapshot(FILE *snapshot_file, struct creo_brl_snapshot_header *header)
{
    const __int64 end_offset = _ftelli64(snapshot_file);

    if (end_offset < 0)
        return 0;
    header->snapshot_size = (uint64_t)end_offset;
    return header->snapshot_size >= sizeof(*header);
}


static int
write_scene_snapshot(const char *snapshot_path, struct snapshot_scene_capture *capture,
                     uint32_t root_type, uint32_t root_index)
{
    struct creo_brl_snapshot_header header = {};
    struct creo_brl_snapshot_scene scene = {};
    FILE *snapshot_file = NULL;
    int result = CREO_BRL_SNAPSHOT_CAPTURE_WRITE_FAILURE;

    if (!snapshot_open_for_write(snapshot_path, &snapshot_file))
        return result;

    memcpy(header.magic, creo_brl_snapshot_magic, sizeof(header.magic));
    scene.settings = capture->settings;
    scene.part_count = capture->parts.size();
    scene.assembly_count = capture->assemblies.size();
    scene.root_type = root_type;
    scene.root_index = root_index;
    if (scene.part_count > 0) {
        scene.parts.offset = sizeof(header) + sizeof(scene);
        scene.parts.size = scene.part_count * sizeof(struct creo_brl_snapshot_part);
    }
    if (scene.assembly_count > 0) {
        scene.assemblies.offset = sizeof(header) + sizeof(scene) + scene.parts.size;
        scene.assemblies.size = scene.assembly_count * sizeof(struct creo_brl_snapshot_assembly);
    }

    if (fwrite(&header, 1, sizeof(header), snapshot_file) != sizeof(header) ||
        fwrite(&scene, 1, sizeof(scene), snapshot_file) != sizeof(scene))
        goto cleanup;
    for (size_t index = 0; index < capture->parts.size(); ++index) {
        if (fwrite(&capture->parts[index].part, 1, sizeof(capture->parts[index].part), snapshot_file) !=
            sizeof(capture->parts[index].part))
            goto cleanup;
    }
    for (size_t index = 0; index < capture->assemblies.size(); ++index) {
        if (fwrite(&capture->assemblies[index].assembly, 1,
                   sizeof(capture->assemblies[index].assembly), snapshot_file) !=
            sizeof(capture->assemblies[index].assembly))
            goto cleanup;
    }
    if (!write_settings_strings(snapshot_file, &scene.settings, capture->settings_strings))
        goto cleanup;
    for (size_t index = 0; index < capture->parts.size(); ++index) {
        if (!write_part_capture(snapshot_file, &capture->parts[index]))
            goto cleanup;
    }
    for (size_t index = 0; index < capture->assemblies.size(); ++index) {
        struct snapshot_assembly_capture *assembly = &capture->assemblies[index];

        assembly->assembly.member_count = assembly->members.size();
        assembly->assembly.parameter_count = assembly->parameters.size();
        if (!write_text(snapshot_file, assembly->model_name, &assembly->assembly.model_name) ||
            !write_text(snapshot_file, assembly->model_version, &assembly->assembly.model_version) ||
            !write_range(snapshot_file, assembly->members.data(),
                         assembly->members.size() * sizeof(assembly->members[0]),
                         &assembly->assembly.members) ||
            !write_named_values(snapshot_file, assembly->parameters,
                                &assembly->assembly.parameters,
                                &assembly->assembly.parameter_count))
            goto cleanup;
    }
    if (!finish_snapshot(snapshot_file, &header) ||
        _fseeki64(snapshot_file, 0, SEEK_SET) != 0 ||
        fwrite(&header, 1, sizeof(header), snapshot_file) != sizeof(header) ||
        fwrite(&scene, 1, sizeof(scene), snapshot_file) != sizeof(scene))
        goto cleanup;
    if (scene.part_count > 0 &&
        _fseeki64(snapshot_file, (__int64)scene.parts.offset, SEEK_SET) != 0)
        goto cleanup;
    for (size_t index = 0; index < capture->parts.size(); ++index) {
        if (fwrite(&capture->parts[index].part, 1, sizeof(capture->parts[index].part), snapshot_file) !=
            sizeof(capture->parts[index].part))
            goto cleanup;
    }
    if (scene.assembly_count > 0 &&
        _fseeki64(snapshot_file, (__int64)scene.assemblies.offset, SEEK_SET) != 0)
        goto cleanup;
    for (size_t index = 0; index < capture->assemblies.size(); ++index) {
        if (fwrite(&capture->assemblies[index].assembly, 1,
                   sizeof(capture->assemblies[index].assembly), snapshot_file) !=
            sizeof(capture->assemblies[index].assembly))
            goto cleanup;
    }

    result = CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS;

cleanup:
    fclose(snapshot_file);
    return result;
}


extern "C" int
creo_brl_frontend_capture_snapshot(const char *snapshot_path)
{
    struct creo_brl_snapshot_settings settings = {};
    struct snapshot_settings_strings settings_strings;
    ProMdl model = NULL;
    ProMdlType model_type;

    if (!snapshot_path || !snapshot_path[0])
        return CREO_BRL_SNAPSHOT_CAPTURE_INVALID_REQUEST;
    if (!capture_settings(&settings, &settings_strings))
        return CREO_BRL_SNAPSHOT_CAPTURE_DIALOG_FAILURE;
    if (ProMdlCurrentGet(&model) != PRO_TK_NO_ERROR)
        return CREO_BRL_SNAPSHOT_CAPTURE_NO_ACTIVE_MODEL;
    if (ProMdlTypeGet(model, &model_type) != PRO_TK_NO_ERROR)
        return CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE;

    if (model_type == PRO_MDL_PART || model_type == PRO_MDL_ASSEMBLY) {
        struct snapshot_scene_capture capture = {};
        uint32_t root_type = 0;
        uint32_t root_index = 0;
        const int result = creo_brl_frontend_capture_model_scene(model, model_type,
                                                                 &settings, &settings_strings,
                                                                 &capture,
                                                                 &root_type, &root_index);

        return result == CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS ?
                   write_scene_snapshot(snapshot_path, &capture, root_type, root_index) : result;
    }

    return CREO_BRL_SNAPSHOT_CAPTURE_UNSUPPORTED_MODEL;
}
