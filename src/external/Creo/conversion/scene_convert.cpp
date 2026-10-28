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


struct snapshot_assembly_input {
    struct creo_brl_snapshot_assembly record;
    std::string model_name;
    std::string model_version;
    std::vector<struct snapshot_named_value> parameters;
};


static int
snapshot_seek(FILE *snapshot_file, uint64_t offset)
{
    if (!snapshot_file || offset > INT64_MAX)
        return 0;

#ifdef _WIN32
    return _fseeki64(snapshot_file, (__int64)offset, SEEK_SET) == 0;
#else
    return fseek(snapshot_file, (long)offset, SEEK_SET) == 0;
#endif
}


static int
snapshot_read(FILE *snapshot_file, void *buffer, size_t size)
{
    return size == 0 || (snapshot_file && buffer && fread(buffer, 1, size, snapshot_file) == size);
}


static int
snapshot_read_at(FILE *snapshot_file, uint64_t offset, void *buffer, size_t size)
{
    return snapshot_seek(snapshot_file, offset) && snapshot_read(snapshot_file, buffer, size);
}


static int
snapshot_count_as_size(uint64_t count, size_t multiplier, size_t *size)
{
    if (!size || multiplier == 0 || count > SIZE_MAX / multiplier)
        return 0;

    *size = (size_t)count * multiplier;
    return 1;
}


static int
snapshot_read_tgc_subtractions(FILE *snapshot_file,
                               const struct creo_brl_snapshot_part *part,
                               const std::string& model_name,
                               struct db_i *database,
                               struct rt_wdb *writer,
                               std::vector<std::string> *subtraction_names)
{
    const std::string hole_name_root = creo_brl_snapshot_name_root(model_name) + "_hole";

    if (!snapshot_file || !part || !database || !writer || !subtraction_names)
        return CREO_BRL_CORE_CONVERT_INVALID_REQUEST;

    subtraction_names->clear();
    if (part->tgc_subtraction_count == 0)
        return CREO_BRL_CORE_CONVERT_SUCCESS;
    if (!snapshot_seek(snapshot_file, part->tgc_subtractions.offset))
        return CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT;

    subtraction_names->reserve((size_t)part->tgc_subtraction_count);
    for (uint64_t index = 0; index < part->tgc_subtraction_count; ++index) {
        struct creo_brl_snapshot_tgc_subtraction subtraction = {};
        std::string solid_name;
        std::string unused_region_name;
        std::string hole_name = hole_name_root;

        if (!snapshot_read(snapshot_file, &subtraction, sizeof(subtraction)))
            return CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT;

        if (part->tgc_subtraction_count > 1)
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
snapshot_read_text(FILE *snapshot_file,
                   const struct creo_brl_snapshot_range *range,
                   std::string *text)
{
    if (!range || !text || range->size > SIZE_MAX)
        return 0;

    text->resize((size_t)range->size);
    return (range->size == 0 ||
            snapshot_read_at(snapshot_file, range->offset, &(*text)[0], text->size())) &&
           text->find('\0') == std::string::npos;
}


static int
snapshot_read_named_values(FILE *snapshot_file,
                           const struct creo_brl_snapshot_range *range,
                           uint64_t count,
                           std::vector<struct snapshot_named_value> *values)
{
    uint64_t index = 0;

    if (!range || !values)
        return 0;
    if (count == 0) {
        values->clear();
        return 1;
    }

    values->clear();
    values->reserve((size_t)count);
    for (index = 0; index < count; ++index) {
        struct creo_brl_snapshot_named_value record = {};
        struct snapshot_named_value value = {};
        const uint64_t offset = range->offset + index * sizeof(record);

        if (!snapshot_read_at(snapshot_file, offset, &record, sizeof(record)) ||
            !snapshot_read_text(snapshot_file, &record.name, &value.name) ||
            !snapshot_read_text(snapshot_file, &record.value, &value.value))
            return 0;
        values->push_back(value);
    }

    return 1;
}


static int
snapshot_read_vectors(FILE *snapshot_file,
                      const struct creo_brl_snapshot_range *range,
                      uint64_t count,
                      std::vector<fastf_t> *vectors)
{
    size_t coordinate_count = 0;
    uint64_t index = 0;

    if (!range || !vectors ||
        !snapshot_count_as_size(count, CREO_BRL_SNAPSHOT_COORDINATE_COUNT, &coordinate_count) ||
        coordinate_count > vectors->max_size() || !snapshot_seek(snapshot_file, range->offset))
        return 0;

    vectors->resize(coordinate_count);
    for (index = 0; index < count; ++index) {
        struct creo_brl_snapshot_vector vector = {};
        size_t coordinate = 0;

        if (!snapshot_read(snapshot_file, &vector, sizeof(vector)))
            return 0;

        for (coordinate = 0; coordinate < CREO_BRL_SNAPSHOT_COORDINATE_COUNT; ++coordinate) {
            if (!std::isfinite(vector.coordinates[coordinate]))
                return 0;
            (*vectors)[(size_t)index * CREO_BRL_SNAPSHOT_COORDINATE_COUNT + coordinate] =
                (fastf_t)vector.coordinates[coordinate];
        }
    }

    return 1;
}


static int
snapshot_read_triangles(FILE *snapshot_file,
                        const struct creo_brl_snapshot_range *range,
                        uint64_t count,
                        uint64_t vertex_count,
                        std::vector<int> *triangles)
{
    size_t index_count = 0;
    uint64_t index = 0;

    if (!range || !triangles ||
        !snapshot_count_as_size(count, CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT, &index_count) ||
        index_count > triangles->max_size() || !snapshot_seek(snapshot_file, range->offset))
        return 0;

    triangles->resize(index_count);
    for (index = 0; index < count; ++index) {
        struct creo_brl_snapshot_triangle triangle = {};
        size_t vertex = 0;

        if (!snapshot_read(snapshot_file, &triangle, sizeof(triangle)))
            return 0;

        for (vertex = 0; vertex < CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT; ++vertex) {
            if ((uint64_t)triangle.vertices[vertex] >= vertex_count || triangle.vertices[vertex] > INT_MAX)
                return 0;
            (*triangles)[(size_t)index * CREO_BRL_SNAPSHOT_TRIANGLE_VERTEX_COUNT + vertex] =
                (int)triangle.vertices[vertex];
        }
    }

    return 1;
}


static int
snapshot_read_indices(FILE *snapshot_file,
                      const struct creo_brl_snapshot_range *range,
                      uint64_t count,
                      uint64_t maximum,
                      std::vector<int> *indices)
{
    size_t index_count = 0;
    uint64_t index = 0;

    if (!range || !indices || !snapshot_count_as_size(count, 1, &index_count) ||
        index_count > indices->max_size() || !snapshot_seek(snapshot_file, range->offset))
        return 0;

    indices->resize(index_count);
    for (index = 0; index < count; ++index) {
        uint32_t value = 0;

        if (!snapshot_read(snapshot_file, &value, sizeof(value)) ||
            (uint64_t)value >= maximum || value > INT_MAX)
            return 0;
        (*indices)[(size_t)index] = (int)value;
    }

    return 1;
}


static int
snapshot_write_part_definition(FILE *snapshot_file,
                               const struct creo_brl_snapshot_part *part,
                               const struct creo_brl_snapshot_settings *settings,
                               const std::string& rename_parameters,
                               const material_translation_map& material_translations,
                               FILE *log_file,
                               struct db_i *database,
                               struct rt_wdb *writer,
                               struct snapshot_part_output *output)
{
    const int has_mesh = (part->flags & CREO_BRL_SNAPSHOT_PART_HAS_MESH) != 0;
    const int has_normals = (part->flags & CREO_BRL_SNAPSHOT_PART_HAS_NORMALS) != 0;
    std::string model_name;
    std::string model_version;
    std::string material_name;
    std::vector<struct snapshot_named_value> attributes;
    std::vector<struct snapshot_named_value> parameters;
    std::vector<std::string> subtraction_names;
    std::vector<fastf_t> vertices;
    std::vector<int> triangles;
    std::vector<fastf_t> normals;
    std::vector<int> normal_indices;
    const struct snapshot_material_translation *material_translation = NULL;

    if (!snapshot_file || !part || !settings)
        return CREO_BRL_CORE_CONVERT_INVALID_REQUEST;

    if (!snapshot_read_text(snapshot_file, &part->model_name, &model_name) ||
        !snapshot_read_text(snapshot_file, &part->model_version, &model_version) ||
        !snapshot_read_text(snapshot_file, &part->material_name, &material_name) ||
        !snapshot_read_named_values(snapshot_file, &part->attributes, part->attribute_count, &attributes) ||
        !snapshot_read_named_values(snapshot_file, &part->parameters, part->parameter_count, &parameters) ||
        (has_mesh &&
         (!snapshot_read_vectors(snapshot_file, &part->vertices, part->vertex_count, &vertices) ||
          !snapshot_read_triangles(snapshot_file, &part->triangles, part->triangle_count,
                                   part->vertex_count, &triangles) ||
          (has_normals &&
            (!snapshot_read_vectors(snapshot_file, &part->normals, part->normal_count, &normals) ||
             !snapshot_read_indices(snapshot_file, &part->normal_indices,
                                    part->normal_index_count, part->normal_count,
                                    &normal_indices))))))
        return CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT;

    {
        const material_translation_map::const_iterator found =
            material_translations.find(creo_brl_name_text_lower(material_name));

        if (found != material_translations.end())
            material_translation = &found->second;
    }

    if (part->tgc_subtraction_count > 0) {
        const int csg_result = snapshot_read_tgc_subtractions(snapshot_file, part,
                                                              model_name,
                                                              database, writer,
                                                              &subtraction_names);

        if (csg_result != CREO_BRL_CORE_CONVERT_SUCCESS)
            return csg_result;
    }

    return creo_brl_convert_snapshot_part(part, settings, rename_parameters,
                                          model_name, model_version, material_name,
                                          material_translation,
                                          attributes, parameters, subtraction_names,
                                          vertices, triangles, normals, normal_indices,
                                          log_file,
                                          database, writer, output);
}


static int
snapshot_write_assembly_attributes(struct db_i *database,
                                   const char *assembly_name,
                                   const struct creo_brl_snapshot_assembly *assembly_record,
                                   const std::string& model_name,
                                   const std::string& model_version,
                                   const std::vector<struct snapshot_named_value>& parameters)
{
    struct bu_attribute_value_set attributes = BU_AVS_INIT_ZERO;
    struct directory *assembly = db_lookup(database, assembly_name, LOOKUP_QUIET);
    int result = 0;

    if (!assembly_record || assembly == RT_DIR_NULL ||
        db5_get_attributes(database, &attributes, assembly) != 0)
        goto cleanup;

    bu_avs_add(&attributes, CREO_BRL_CORE_IMPORTER_ATTRIBUTE, CREO_BRL_CORE_IMPORTER_VALUE);
    bu_avs_add(&attributes, CREO_BRL_CORE_MODEL_NAME_ATTRIBUTE, model_name.c_str());
    if (!model_version.empty())
        bu_avs_add(&attributes, CREO_BRL_CORE_MODEL_VERSION_ATTRIBUTE, model_version.c_str());
    creo_brl_add_snapshot_named_values(&attributes, parameters);
    creo_brl_add_snapshot_mass_properties(
        &attributes,
        (assembly_record->flags & CREO_BRL_SNAPSHOT_ASSEMBLY_HAS_MASS_PROPERTIES) != 0,
        assembly_record->density, assembly_record->mass,
        assembly_record->volume, assembly_record->surface_area);
    result = db5_update_attributes(assembly, &attributes, database) == 0;

cleanup:
    bu_avs_free(&attributes);
    return result;
}


static int
snapshot_write_assembly(FILE *snapshot_file,
                        const struct snapshot_assembly_input *assembly,
                        const std::vector<struct snapshot_part_output>& part_outputs,
                        const std::vector<std::string>& assembly_names,
                        const std::vector<int>& assembly_written,
                        struct db_i *database,
                        struct rt_wdb *writer,
                        const std::string& assembly_name)
{
    const struct creo_brl_snapshot_assembly *assembly_record =
        assembly ? &assembly->record : NULL;
    struct wmember members;
    uint64_t member_index = 0;

    if (!assembly_record)
        return CREO_BRL_CORE_CONVERT_INVALID_REQUEST;

    BU_LIST_INIT(&members.l);
    for (member_index = 0; member_index < assembly_record->member_count; ++member_index) {
        struct creo_brl_snapshot_assembly_member member = {};
        const uint64_t offset = assembly_record->members.offset +
                                member_index * sizeof(struct creo_brl_snapshot_assembly_member);
        const char *target_name = NULL;
        mat_t matrix = {};

        if (!snapshot_read_at(snapshot_file, offset, &member, sizeof(member))) {
            mk_freemembers(&members.l);
            return CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT;
        }

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
        !snapshot_write_assembly_attributes(database, assembly_name.c_str(), assembly_record,
                                            assembly->model_name, assembly->model_version,
                                            assembly->parameters))
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
creo_brl_convert_snapshot_scene(FILE *snapshot_file,
                                const struct creo_brl_snapshot_scene *scene)
{
    std::string output_path;
    std::string rename_parameters;
    std::vector<struct snapshot_part_output> part_outputs;
    std::vector<struct snapshot_assembly_input> assemblies;
    std::vector<std::string> assembly_names;
    std::vector<int> assembly_written;
    std::vector<std::string> reserved_names;
    material_translation_map material_translations;
    struct db_i *database = DBI_NULL;
    struct rt_wdb *writer = RT_WDB_NULL;
    FILE *log_file = NULL;
    int result = CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;

    if (!snapshot_file || !scene)
        return CREO_BRL_CORE_CONVERT_INVALID_REQUEST;

    try {
        std::string log_path;
        std::string material_path;

        if (!snapshot_read_text(snapshot_file, &scene->settings.output_path, &output_path) ||
            !snapshot_read_text(snapshot_file, &scene->settings.log_path, &log_path) ||
            !snapshot_read_text(snapshot_file, &scene->settings.material_path, &material_path) ||
            !snapshot_read_text(snapshot_file, &scene->settings.rename_parameters,
                                &rename_parameters)) {
            result = CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT;
            goto cleanup;
        }
        if (scene->settings.log_type != CREO_BRL_SNAPSHOT_LOG_NONE &&
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

        part_outputs.resize((size_t)scene->part_count);
        for (uint64_t index = 0; index < scene->part_count; ++index) {
            struct creo_brl_snapshot_part part = {};
            const uint64_t offset = scene->parts.offset + index * sizeof(struct creo_brl_snapshot_part);

            if (!snapshot_read_at(snapshot_file, offset, &part, sizeof(part))) {
                result = CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT;
                goto cleanup;
            }
            result = snapshot_write_part_definition(snapshot_file, &part, &scene->settings,
                                                    rename_parameters,
                                                    material_translations,
                                                    log_file, database, writer,
                                                    &part_outputs[(size_t)index]);
            if (result != CREO_BRL_CORE_CONVERT_SUCCESS)
                goto cleanup;
        }

        assemblies.resize((size_t)scene->assembly_count);
        assembly_names.resize((size_t)scene->assembly_count);
        assembly_written.assign((size_t)scene->assembly_count, 0);
        for (uint64_t index = 0; index < scene->assembly_count; ++index) {
            struct snapshot_assembly_input& assembly = assemblies[(size_t)index];
            const uint64_t offset = scene->assemblies.offset + index * sizeof(struct creo_brl_snapshot_assembly);
            std::string name_source;

            if (!snapshot_read_at(snapshot_file, offset, &assembly.record,
                                  sizeof(assembly.record)) ||
                !snapshot_read_text(snapshot_file, &assembly.record.model_name,
                                    &assembly.model_name) ||
                !snapshot_read_text(snapshot_file, &assembly.record.model_version,
                                    &assembly.model_version) ||
                !snapshot_read_named_values(snapshot_file, &assembly.record.parameters,
                                            assembly.record.parameter_count,
                                            &assembly.parameters)) {
                result = CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT;
                goto cleanup;
            }

            name_source = creo_brl_snapshot_name_source(
                rename_parameters, assembly.model_name, assembly.parameters);
            if (!creo_brl_snapshot_unique_name(database,
                                               creo_brl_snapshot_name_root(name_source),
                                               reserved_names, &assembly_names[(size_t)index])) {
                result = CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;
                goto cleanup;
            }
            reserved_names.push_back(assembly_names[(size_t)index]);
        }

        for (uint64_t remaining = scene->assembly_count; remaining > 0;) {
            int wrote_assembly = 0;

            for (uint64_t index = 0; index < scene->assembly_count; ++index) {
                const struct snapshot_assembly_input *assembly = &assemblies[(size_t)index];
                const struct creo_brl_snapshot_assembly *assembly_record = &assembly->record;
                int dependencies_written = 1;

                if (assembly_written[(size_t)index])
                    continue;
                for (uint64_t member_index = 0;
                     member_index < assembly_record->member_count; ++member_index) {
                    struct creo_brl_snapshot_assembly_member member = {};
                    const uint64_t offset = assembly_record->members.offset +
                                            member_index * sizeof(member);

                    if (!snapshot_read_at(snapshot_file, offset, &member, sizeof(member))) {
                        result = CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT;
                        goto cleanup;
                    }
                    if (member.target_type == CREO_BRL_SNAPSHOT_SCENE_NODE_ASSEMBLY &&
                        !assembly_written[member.target_index]) {
                        dependencies_written = 0;
                        break;
                    }
                }
                if (!dependencies_written)
                    continue;

                result = snapshot_write_assembly(snapshot_file, assembly, part_outputs,
                                                 assembly_names, assembly_written,
                                                 database, writer, assembly_names[(size_t)index]);
                if (result != CREO_BRL_CORE_CONVERT_SUCCESS)
                    goto cleanup;
                assembly_written[(size_t)index] = 1;
                --remaining;
                wrote_assembly = 1;
            }

            if (!wrote_assembly) {
                result = CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT;
                goto cleanup;
            }
        }

        {
            const char *root_name = scene->root_type == CREO_BRL_SNAPSHOT_SCENE_NODE_PART ?
                                        part_outputs[scene->root_index].region_name.c_str() :
                                        assembly_names[scene->root_index].c_str();
            std::string top_level_name;
            struct wmember members;
            mat_t orientation;

            if (!creo_brl_snapshot_unique_name(database, CREO_BRL_CORE_TOP_LEVEL_NAME,
                                               reserved_names, &top_level_name)) {
                result = CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;
                goto cleanup;
            }
            snapshot_orientation_matrix(scene->settings.xform_mode, orientation);
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
