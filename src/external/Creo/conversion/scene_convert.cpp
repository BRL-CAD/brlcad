/**
 * Snapshot scene replay and output policy helpers for the runtime core.
 */

#include "common.h"

#include "conversion/scene_convert.h"

#include <cmath>
#include <cstring>
#include <limits.h>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#  include <windows.h>
#endif

#include "bu/avs.h"
#include "bu/file.h"
#include "conversion/mesh_convert.h"
#include "conversion/name_allocator.h"
#include "raytrace.h"
#include "shared/core_api.h"
#include "shared/name_selection.h"
#include "wdb.h"


static const char CREO_BRL_CORE_TOP_LEVEL_NAME[] = "all";
static const char CREO_BRL_CORE_HOLE_SUFFIX[] = ".s";
static const size_t CREO_BRL_MATERIAL_LINE_SIZE = 4096u;
static const char CREO_BRL_MATERIAL_COMMENT_CHARACTERS[] = "!@#$%^&*/<>?";


typedef std::map<std::string, struct snapshot_material_translation> material_translation_map;


static int
snapshot_write_tgc_subtractions(const struct snapshot_part_capture& part_capture,
                                struct db_i *database,
                                struct rt_wdb *writer,
                                std::vector<std::string> *subtraction_names)
{
    const std::vector<struct creo_brl_snapshot_tgc_subtraction>& subtractions =
        part_capture.tgc_subtractions;
    const std::string hole_name_root =
        creo_brl_snapshot_name_root(part_capture.strings.model_name) + "_hole";

    if (!database || !writer || !subtraction_names)
        return CREO_BRL_CORE_CONVERT_INVALID_REQUEST;

    subtraction_names->clear();
    if (subtractions.empty())
        return CREO_BRL_CORE_CONVERT_SUCCESS;

    subtraction_names->reserve(subtractions.size());
    for (size_t index = 0; index < subtractions.size(); ++index) {
        const struct creo_brl_snapshot_tgc_subtraction& subtraction = subtractions[index];
        std::string solid_name;
        std::string unused_region_name;
        std::string hole_name = hole_name_root;

        if (subtractions.size() > 1)
            hole_name += "_" + std::to_string(index + 1u);
        if (!creo_brl_snapshot_object_names(database, hole_name, CREO_BRL_CORE_HOLE_SUFFIX,
                                            &solid_name, &unused_region_name) ||
            mk_tgc(writer, solid_name.c_str(),
                   subtraction.base.coordinates,
                   subtraction.height.coordinates,
                   subtraction.a.coordinates,
                   subtraction.b.coordinates,
                   subtraction.c.coordinates,
                   subtraction.d.coordinates) != 0)
            return CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;

        subtraction_names->push_back(solid_name);
    }

    return CREO_BRL_CORE_CONVERT_SUCCESS;
}


static int
snapshot_open_output_database(const char *output_path, struct db_i **database)
{
    const int database_exists = output_path && bu_file_exists(output_path, NULL);

    if (!output_path || !database)
        return 0;

    if (database_exists)
        *database = db_open(output_path, DB_OPEN_READWRITE);
    else
        *database = db_create(output_path, BRLCAD_DB_FORMAT_LATEST);

    return *database != DBI_NULL &&
           (!database_exists || db_dirbuild(*database) >= 0);
}


static FILE *
snapshot_open_utf8_file(const char *path, const wchar_t *wide_mode, const char *mode)
{
    if (!path || !path[0] || !mode)
        return NULL;

#ifdef _WIN32
    {
        const int wide_path_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                                         path, -1, NULL, 0);
        std::vector<wchar_t> wide_path;

        if (!wide_mode || wide_path_length <= 0)
            return NULL;

        wide_path.resize((size_t)wide_path_length);
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1,
                                &wide_path[0], wide_path_length) != wide_path_length)
            return NULL;

        return _wfopen(&wide_path[0], wide_mode);
    }
#else
    return fopen(path, mode);
#endif
}


static int
snapshot_open_log_file(const char *log_path, FILE **log_file)
{
    if (!log_file)
        return 0;

    *log_file = NULL;
    if (!log_path || log_path[0] == '\0')
        return 1;

    *log_file = snapshot_open_utf8_file(log_path, L"wb", "wb");
    return *log_file != NULL;
}


static void
load_material_translations(const std::string& path, material_translation_map *translations)
{
    char line[CREO_BRL_MATERIAL_LINE_SIZE] = {'\0'};
    FILE *material_file = NULL;

    if (!translations)
        return;
    translations->clear();
    if (path.empty() ||
        !(material_file = snapshot_open_utf8_file(path.c_str(), L"rb", "rb")))
        return;

    while (fgets(line, sizeof(line), material_file)) {
        std::istringstream input(line);
        std::string material_name;
        struct snapshot_material_translation translation = {};

        if (!(input >> material_name) ||
            strchr(CREO_BRL_MATERIAL_COMMENT_CHARACTERS, material_name[0]) ||
            !(input >> translation.material_id >> translation.los))
            continue;

        translations->insert(std::make_pair(creo_brl_name_text_lower(material_name), translation));
    }

    fclose(material_file);
}


static int
snapshot_write_part_definition(const struct snapshot_part_capture& part_capture,
                               const struct snapshot_scene_capture& scene,
                               const material_translation_map& material_translations,
                               FILE *log_file,
                               struct db_i *database,
                               struct rt_wdb *writer,
                               struct snapshot_part_output *output)
{
    std::vector<std::string> subtraction_names;
    const struct snapshot_material_translation *material_translation = NULL;

    {
        const material_translation_map::const_iterator found =
            material_translations.find(
                creo_brl_name_text_lower(part_capture.strings.material_name));

        if (found != material_translations.end())
            material_translation = &found->second;
    }

    if (!part_capture.tgc_subtractions.empty()) {
        const int csg_result = snapshot_write_tgc_subtractions(
            part_capture, database, writer, &subtraction_names);

        if (csg_result != CREO_BRL_CORE_CONVERT_SUCCESS)
            return csg_result;
    }

    return creo_brl_convert_snapshot_part(part_capture, scene,
                                          material_translation,
                                          subtraction_names,
                                          log_file,
                                          database, writer, output);
}


static int
snapshot_write_assembly_attributes(struct db_i *database,
                                   const char *assembly_name,
                                   const struct snapshot_assembly_capture& assembly_capture)
{
    const struct creo_brl_snapshot_assembly& assembly_record = assembly_capture.assembly;
    struct bu_attribute_value_set attributes = BU_AVS_INIT_ZERO;
    struct directory *assembly = db_lookup(database, assembly_name, LOOKUP_QUIET);
    int result = 0;

    if (assembly == RT_DIR_NULL ||
        db5_get_attributes(database, &attributes, assembly) != 0)
        goto cleanup;

    bu_avs_add(&attributes, CREO_BRL_CORE_IMPORTER_ATTRIBUTE, CREO_BRL_CORE_IMPORTER_VALUE);
    bu_avs_add(&attributes, CREO_BRL_CORE_MODEL_NAME_ATTRIBUTE, assembly_capture.model_name.c_str());
    if (!assembly_capture.model_version.empty()) {
        bu_avs_add(&attributes, CREO_BRL_CORE_MODEL_VERSION_ATTRIBUTE,
                   assembly_capture.model_version.c_str());
    }
    creo_brl_add_snapshot_named_values(&attributes, assembly_capture.parameters);
    creo_brl_add_snapshot_mass_properties(
        &attributes,
        (assembly_record.flags & CREO_BRL_SNAPSHOT_ASSEMBLY_HAS_MASS_PROPERTIES) != 0,
        assembly_record.density, assembly_record.mass,
        assembly_record.volume, assembly_record.surface_area);
    result = db5_update_attributes(assembly, &attributes, database) == 0;

cleanup:
    bu_avs_free(&attributes);
    return result;
}


static int
snapshot_write_assembly(const struct snapshot_assembly_capture& assembly,
                        const std::vector<struct snapshot_part_output>& part_outputs,
                        const std::vector<std::string>& assembly_names,
                        const std::vector<int>& assembly_written,
                        struct db_i *database,
                        struct rt_wdb *writer,
                        const std::string& assembly_name)
{
    struct wmember members;

    BU_LIST_INIT(&members.l);
    for (size_t member_index = 0; member_index < assembly.members.size(); ++member_index) {
        const struct creo_brl_snapshot_assembly_member& member = assembly.members[member_index];
        const char *target_name = NULL;
        mat_t matrix = {};

        if (member.target_type == CREO_BRL_SNAPSHOT_SCENE_NODE_PART) {
            target_name = part_outputs[member.target_index].region_name.c_str();
        } else {
            if (!assembly_written[member.target_index]) {
                mk_freemembers(&members.l);
                return CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;
            }
            target_name = assembly_names[member.target_index].c_str();
        }

        for (unsigned int element = 0; element < CREO_BRL_SNAPSHOT_MATRIX_ELEMENT_COUNT; ++element)
            matrix[element] = (fastf_t)member.matrix.elements[element];
        if (mk_addmember(target_name, &members.l, matrix, WMOP_UNION) == WMEMBER_NULL) {
            mk_freemembers(&members.l);
            return CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;
        }
    }

    if (mk_lcomb(writer, assembly_name.c_str(), &members, 0, NULL, NULL, NULL, 0) != 0 ||
        !snapshot_write_assembly_attributes(database, assembly_name.c_str(), assembly))
        return CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;

    return CREO_BRL_CORE_CONVERT_SUCCESS;
}


static void
snapshot_orientation_matrix(int32_t xform_mode, mat_t matrix)
{
    static const fastf_t identity[CREO_BRL_SNAPSHOT_MATRIX_ELEMENT_COUNT] = {
        1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0
    };
    static const fastf_t x_to_z[CREO_BRL_SNAPSHOT_MATRIX_ELEMENT_COUNT] = {
        0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0,
        1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0
    };
    static const fastf_t y_to_z[CREO_BRL_SNAPSHOT_MATRIX_ELEMENT_COUNT] = {
        0.0, 0.0, 1.0, 0.0, 1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0
    };
    const fastf_t *source = identity;

    if (xform_mode == CREO_BRL_SNAPSHOT_XFORM_X_TO_Z)
        source = x_to_z;
    else if (xform_mode == CREO_BRL_SNAPSHOT_XFORM_Y_TO_Z)
        source = y_to_z;

    memcpy(matrix, source, sizeof(identity));
}


int
creo_brl_convert_snapshot_scene(const struct snapshot_scene_capture& scene)
{
    const std::string& output_path = scene.settings_strings.output_path;
    const std::string& log_path = scene.settings_strings.log_path;
    const std::string& material_path = scene.settings_strings.material_path;
    const std::string& rename_parameters = scene.settings_strings.rename_parameters;
    std::vector<struct snapshot_part_output> part_outputs;
    std::vector<std::string> assembly_names;
    std::vector<int> assembly_written;
    std::vector<std::string> reserved_names;
    material_translation_map material_translations;
    struct db_i *database = DBI_NULL;
    struct rt_wdb *writer = RT_WDB_NULL;
    FILE *log_file = NULL;
    int result = CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;

    try {
        if (scene.settings.log_type != CREO_BRL_SNAPSHOT_LOG_NONE &&
            !snapshot_open_log_file(log_path.c_str(), &log_file)) {
            result = CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;
            goto cleanup;
        }
        load_material_translations(material_path, &material_translations);
        if (!snapshot_open_output_database(output_path.c_str(), &database) ||
            !(writer = wdb_dbopen(database, RT_WDB_TYPE_DB_DISK))) {
            result = CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;
            goto cleanup;
        }

        part_outputs.resize(scene.parts.size());
        for (size_t index = 0; index < scene.parts.size(); ++index) {
            result = snapshot_write_part_definition(scene.parts[index], scene,
                                                    material_translations,
                                                    log_file, database, writer,
                                                    &part_outputs[index]);
            if (result != CREO_BRL_CORE_CONVERT_SUCCESS)
                goto cleanup;
        }

        assembly_names.resize(scene.assemblies.size());
        assembly_written.assign(scene.assemblies.size(), 0);
        for (size_t index = 0; index < scene.assemblies.size(); ++index) {
            const struct snapshot_assembly_capture& assembly = scene.assemblies[index];
            const std::string name_source = creo_brl_snapshot_name_source(
                rename_parameters, assembly.model_name, assembly.parameters);

            if (!creo_brl_snapshot_unique_name(database,
                                               creo_brl_snapshot_name_root(name_source),
                                               reserved_names, &assembly_names[index])) {
                result = CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;
                goto cleanup;
            }
            reserved_names.push_back(assembly_names[index]);
        }

        for (size_t remaining = scene.assemblies.size(); remaining > 0;) {
            int wrote_assembly = 0;

            for (size_t index = 0; index < scene.assemblies.size(); ++index) {
                const struct snapshot_assembly_capture& assembly = scene.assemblies[index];
                int dependencies_written = 1;

                if (assembly_written[index])
                    continue;
                for (size_t member_index = 0;
                     member_index < assembly.members.size(); ++member_index) {
                    const struct creo_brl_snapshot_assembly_member& member =
                        assembly.members[member_index];

                    if (member.target_type == CREO_BRL_SNAPSHOT_SCENE_NODE_ASSEMBLY &&
                        !assembly_written[member.target_index]) {
                        dependencies_written = 0;
                        break;
                    }
                }
                if (!dependencies_written)
                    continue;

                result = snapshot_write_assembly(assembly, part_outputs,
                                                 assembly_names, assembly_written,
                                                 database, writer, assembly_names[index]);
                if (result != CREO_BRL_CORE_CONVERT_SUCCESS)
                    goto cleanup;
                assembly_written[index] = 1;
                --remaining;
                wrote_assembly = 1;
            }

            if (!wrote_assembly) {
                result = CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT;
                goto cleanup;
            }
        }

        {
            const char *root_name = scene.root_type == CREO_BRL_SNAPSHOT_SCENE_NODE_PART ?
                                        part_outputs[scene.root_index].region_name.c_str() :
                                        assembly_names[scene.root_index].c_str();
            std::string top_level_name;
            struct wmember members;
            mat_t orientation;

            if (!creo_brl_snapshot_unique_name(database, CREO_BRL_CORE_TOP_LEVEL_NAME,
                                               reserved_names, &top_level_name)) {
                result = CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;
                goto cleanup;
            }
            snapshot_orientation_matrix(scene.settings.xform_mode, orientation);
            BU_LIST_INIT(&members.l);
            if (mk_addmember(root_name, &members.l, orientation, WMOP_UNION) == WMEMBER_NULL ||
                mk_lcomb(writer, top_level_name.c_str(), &members, 0, NULL, NULL, NULL, 0) != 0) {
                mk_freemembers(&members.l);
                result = CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;
                goto cleanup;
            }
        }

        result = CREO_BRL_CORE_CONVERT_SUCCESS;
    } catch (...) {
        result = CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;
    }

cleanup:
    if (log_file)
        fclose(log_file);
    if (database != DBI_NULL)
        db_close(database);
    return result;
}


extern "C" CREO_BRL_CORE_EXPORT int CREO_BRL_CORE_CALL
creo_brl_core_convert(const char *snapshot_path)
{
    struct snapshot_scene_capture scene = {};
    const int io_result = creo_brl_snapshot_read(snapshot_path, &scene);

    switch (io_result) {
        case CREO_BRL_SNAPSHOT_IO_SUCCESS:
            return creo_brl_convert_snapshot_scene(scene);
        case CREO_BRL_SNAPSHOT_IO_INVALID_REQUEST:
            return CREO_BRL_CORE_CONVERT_INVALID_REQUEST;
        case CREO_BRL_SNAPSHOT_IO_OPEN_FAILED:
            return CREO_BRL_CORE_CONVERT_OPEN_FAILED;
        default:
            return CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT;
    }
}
