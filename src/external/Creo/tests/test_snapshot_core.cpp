#include "common.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "bu/avs.h"
#include "conversion_snapshot.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/nongeom.h"
#include "rt/tree.h"
#include "shared/core_api.h"


static const char FIXTURE_MODEL_NAME[] = "fixture_part";
static const char FIXTURE_MODEL_VERSION[] = "phase0";
static const char FIXTURE_MATERIAL_NAME[] = "fixture_material";
static const char FIXTURE_SOLID_NAME[] = "fixture_part.s";
static const char FIXTURE_REGION_NAME[] = "fixture_part.r";
static const char FIXTURE_TOP_LEVEL_NAME[] = "all";
static const char FIXTURE_SNAPSHOT_NAME[] = "creo_phase0_v2.snapshot";
static const char MALFORMED_SNAPSHOT_NAME[] = "creo_phase0_malformed.snapshot";
static const char UNSUPPORTED_SNAPSHOT_NAME[] = "creo_phase0_unsupported.snapshot";
static const char OUTPUT_DATABASE_NAME[] = "creo_phase0_v2.g";
static const char EMPTY_REQUEST[] = "";
static const char IMPORTER_ATTRIBUTE[] = "importer";
static const char MODEL_NAME_ATTRIBUTE[] = "ptc_name";
static const char MODEL_VERSION_ATTRIBUTE[] = "ptc_version_stamp";
static const char MATERIAL_ATTRIBUTE[] = "ptc_material_name";
static const char IMPORTER_VALUE[] = "creo-g";
static const unsigned char FIXTURE_COLOR[] = {17u, 34u, 51u};
static const char MANIFEST_LINE_ENDING = 10;
static const uint64_t FIXTURE_PART_COUNT = 1u;
static const uint64_t FIXTURE_VERTEX_COUNT = 3u;
static const uint64_t FIXTURE_TRIANGLE_COUNT = 1u;
static const int FIXTURE_REGION_ID = 42;


static int
test_failure(const char *message)
{
    std::fprintf(stderr, "%s\n", message);
    return 0;
}


static std::string
path_join(const std::string& directory, const char *name)
{
    if (directory.empty())
        return name;

    const char last_character = directory[directory.size() - 1];
    return (last_character == '/' || last_character == '\\') ?
               directory + name :
               directory + "/" + name;
}


static int
append_data(std::vector<unsigned char> *contents,
            const void *data,
            size_t data_size,
            struct creo_brl_snapshot_range *range)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);

    if (!contents || !range)
        return 0;

    if (data_size == 0) {
        range->offset = 0;
        range->size = 0;
        return 1;
    }

    range->offset = (uint64_t)contents->size();
    range->size = (uint64_t)data_size;
    contents->insert(contents->end(), bytes, bytes + data_size);
    return 1;
}


static int
append_record(std::vector<unsigned char> *contents, const void *record, size_t record_size)
{
    struct creo_brl_snapshot_range unused_range = {};

    return append_data(contents, record, record_size, &unused_range);
}


static int
replace_record(std::vector<unsigned char> *contents,
               size_t offset,
               const void *record,
               size_t record_size)
{
    if (!contents || !record || offset > contents->size() ||
        record_size > contents->size() - offset)
        return 0;

    std::memcpy(&(*contents)[offset], record, record_size);
    return 1;
}


static int
write_file(const std::string& path, const std::vector<unsigned char>& contents)
{
    FILE *file = std::fopen(path.c_str(), "wb");
    const int success = file &&
                        std::fwrite(contents.data(), 1, contents.size(), file) == contents.size();

    if (file)
        std::fclose(file);

    return success;
}


static int
write_snapshot_fixture(const std::string& snapshot_path,
                       const std::string& output_path,
                       uint32_t format_version)
{
    struct creo_brl_snapshot_header header = {};
    struct creo_brl_snapshot_scene scene = {};
    struct creo_brl_snapshot_part part = {};
    const struct creo_brl_snapshot_vector vertices[] = {
        {{0.0, 0.0, 0.0}},
        {{100.0, 0.0, 0.0}},
        {{0.0, 100.0, 0.0}}
    };
    const struct creo_brl_snapshot_triangle triangles[] = {{{0u, 1u, 2u}}};
    std::vector<unsigned char> contents;
    const size_t header_offset = 0;
    const size_t scene_offset = sizeof(header);
    const size_t part_offset = sizeof(header) + sizeof(scene);

    std::memcpy(header.magic, creo_brl_snapshot_magic, sizeof(header.magic));
    header.format_version = format_version;
    header.header_size = CREO_BRL_SNAPSHOT_HEADER_V1_SIZE;
    header.payload_type = CREO_BRL_SNAPSHOT_PAYLOAD_SCENE;

    scene.structure_size = CREO_BRL_SNAPSHOT_SCENE_V2_SIZE;
    scene.settings.structure_size = CREO_BRL_SNAPSHOT_SETTINGS_V1_SIZE;
    scene.settings.log_type = CREO_BRL_SNAPSHOT_LOG_NONE;
    scene.part_count = FIXTURE_PART_COUNT;
    scene.parts.offset = part_offset;
    scene.parts.size = sizeof(part);
    scene.root_type = CREO_BRL_SNAPSHOT_SCENE_NODE_PART;

    part.structure_size = CREO_BRL_SNAPSHOT_PART_V1_SIZE;
    part.flags = CREO_BRL_SNAPSHOT_PART_HAS_MESH | CREO_BRL_SNAPSHOT_PART_HAS_COLOR |
                 CREO_BRL_SNAPSHOT_PART_HAS_MATERIAL;
    part.region_id = FIXTURE_REGION_ID;
    part.model_to_mm = 1.0;
    std::memcpy(part.color, FIXTURE_COLOR, sizeof(part.color));
    part.color_is_set = 1;
    part.bbox_max.coordinates[0] = 100.0;
    part.bbox_max.coordinates[1] = 100.0;

    if (!append_record(&contents, &header, sizeof(header)) ||
        !append_record(&contents, &scene, sizeof(scene)) ||
        !append_record(&contents, &part, sizeof(part)) ||
        !append_data(&contents, output_path.data(), output_path.size(), &scene.settings.output_path) ||
        !append_data(&contents, FIXTURE_MODEL_NAME, std::strlen(FIXTURE_MODEL_NAME), &part.model_name) ||
        !append_data(&contents, FIXTURE_MODEL_VERSION, std::strlen(FIXTURE_MODEL_VERSION),
                     &part.model_version) ||
        !append_data(&contents, FIXTURE_MATERIAL_NAME, std::strlen(FIXTURE_MATERIAL_NAME),
                     &part.material_name) ||
        !append_data(&contents, vertices, sizeof(vertices), &part.vertices) ||
        !append_data(&contents, triangles, sizeof(triangles), &part.triangles))
        return 0;

    part.vertex_count = FIXTURE_VERTEX_COUNT;
    part.triangle_count = FIXTURE_TRIANGLE_COUNT;
    header.snapshot_size = (uint64_t)contents.size();
    header.payload_size = header.snapshot_size - header.header_size;

    return replace_record(&contents, header_offset, &header, sizeof(header)) &&
           replace_record(&contents, scene_offset, &scene, sizeof(scene)) &&
           replace_record(&contents, part_offset, &part, sizeof(part)) &&
           write_file(snapshot_path, contents);
}


static int
write_malformed_snapshot(const std::string& snapshot_path)
{
    const std::vector<unsigned char> contents(1u, 0u);

    return write_file(snapshot_path, contents);
}


static int
combination_has_leaf(struct db_i *database, const char *combination_name, const char *leaf_name)
{
    struct directory *directory = db_lookup(database, combination_name, LOOKUP_QUIET);
    struct rt_db_internal internal = RT_DB_INTERNAL_INIT_ZERO;
    int result = 0;

    if (directory == RT_DIR_NULL || !(directory->d_flags & RT_DIR_COMB) ||
        rt_db_get_internal(&internal, directory, database, NULL) < 0 ||
        internal.idb_type != ID_COMBINATION)
        return 0;

    struct rt_comb_internal *combination =
        static_cast<struct rt_comb_internal *>(internal.idb_ptr);
    result = db_find_named_leaf(combination->tree, leaf_name) != TREE_NULL;
    rt_db_free_internal(&internal);
    return result;
}


static int
read_attribute(struct db_i *database,
               const char *object_name,
               const char *attribute_name,
               std::string *value)
{
    struct directory *directory = db_lookup(database, object_name, LOOKUP_QUIET);
    struct bu_attribute_value_set attributes = BU_AVS_INIT_ZERO;
    const char *attribute_value = NULL;
    int result = 0;

    if (directory == RT_DIR_NULL || db5_get_attributes(database, &attributes, directory) != 0)
        goto cleanup;

    attribute_value = bu_avs_get(&attributes, attribute_name);
    if (attribute_value) {
        *value = attribute_value;
        result = 1;
    }

cleanup:
    bu_avs_free(&attributes);
    return result;
}


static int
normalize_output_manifest(const std::string& output_path, std::string *manifest)
{
    struct db_i *database = db_open(output_path.c_str(), DB_OPEN_READONLY);
    struct directory *solid_directory = RT_DIR_NULL;
    struct directory *region_directory = RT_DIR_NULL;
    struct rt_db_internal solid_internal = RT_DB_INTERNAL_INIT_ZERO;
    struct rt_db_internal region_internal = RT_DB_INTERNAL_INIT_ZERO;
    std::string solid_importer;
    std::string solid_model_name;
    std::string region_model_name;
    std::string region_model_version;
    std::string region_material_name;
    int result = 0;

    if (database == DBI_NULL || db_dirbuild(database) < 0)
        goto cleanup;

    solid_directory = db_lookup(database, FIXTURE_SOLID_NAME, LOOKUP_QUIET);
    region_directory = db_lookup(database, FIXTURE_REGION_NAME, LOOKUP_QUIET);
    if (solid_directory == RT_DIR_NULL || region_directory == RT_DIR_NULL ||
        rt_db_get_internal(&solid_internal, solid_directory, database, NULL) < 0 ||
        solid_internal.idb_type != ID_BOT ||
        rt_db_get_internal(&region_internal, region_directory, database, NULL) < 0 ||
        region_internal.idb_type != ID_COMBINATION ||
        !combination_has_leaf(database, FIXTURE_TOP_LEVEL_NAME, FIXTURE_REGION_NAME) ||
        !combination_has_leaf(database, FIXTURE_REGION_NAME, FIXTURE_SOLID_NAME) ||
        !read_attribute(database, FIXTURE_SOLID_NAME, IMPORTER_ATTRIBUTE, &solid_importer) ||
        !read_attribute(database, FIXTURE_SOLID_NAME, MODEL_NAME_ATTRIBUTE, &solid_model_name) ||
        !read_attribute(database, FIXTURE_REGION_NAME, MODEL_NAME_ATTRIBUTE, &region_model_name) ||
        !read_attribute(database, FIXTURE_REGION_NAME, MODEL_VERSION_ATTRIBUTE, &region_model_version) ||
        !read_attribute(database, FIXTURE_REGION_NAME, MATERIAL_ATTRIBUTE, &region_material_name) ||
        solid_importer != IMPORTER_VALUE)
        goto cleanup;

    {
        struct rt_bot_internal *bot = static_cast<struct rt_bot_internal *>(solid_internal.idb_ptr);
        struct rt_comb_internal *region =
            static_cast<struct rt_comb_internal *>(region_internal.idb_ptr);

        if (bot->num_vertices != FIXTURE_VERTEX_COUNT || bot->num_faces != FIXTURE_TRIANGLE_COUNT ||
            bot->num_normals != 0 || !region->region_flag ||
            region->region_id != FIXTURE_REGION_ID || !region->rgb_valid ||
            std::memcmp(region->rgb, FIXTURE_COLOR, sizeof(region->rgb)) != 0)
            goto cleanup;

        manifest->append("fixture=v2_single_mesh");
        manifest->push_back(MANIFEST_LINE_ENDING);
        manifest->append("input=synthetic_v2_scene");
        manifest->push_back(MANIFEST_LINE_ENDING);
        manifest->append("all=combination leaf=");
        manifest->append(FIXTURE_REGION_NAME);
        manifest->push_back(MANIFEST_LINE_ENDING);
        manifest->append("fixture_part.s=bot vertices=");
        manifest->append(std::to_string(bot->num_vertices));
        manifest->append(" faces=");
        manifest->append(std::to_string(bot->num_faces));
        manifest->append(" normals=");
        manifest->append(std::to_string(bot->num_normals));
        manifest->append(" importer=");
        manifest->append(solid_importer);
        manifest->append(" ptc_name=");
        manifest->append(solid_model_name);
        manifest->push_back(MANIFEST_LINE_ENDING);
        manifest->append("fixture_part.r=region id=");
        manifest->append(std::to_string(region->region_id));
        manifest->append(" rgb=");
        manifest->append(std::to_string((unsigned int)region->rgb[0]));
        manifest->push_back(',');
        manifest->append(std::to_string((unsigned int)region->rgb[1]));
        manifest->push_back(',');
        manifest->append(std::to_string((unsigned int)region->rgb[2]));
        manifest->append(" leaf=");
        manifest->append(FIXTURE_SOLID_NAME);
        manifest->append(" ptc_name=");
        manifest->append(region_model_name);
        manifest->append(" ptc_version_stamp=");
        manifest->append(region_model_version);
        manifest->append(" material=");
        manifest->append(region_material_name);
        manifest->push_back(MANIFEST_LINE_ENDING);
    }

    result = 1;

cleanup:
    if (solid_internal.idb_ptr)
        rt_db_free_internal(&solid_internal);
    if (region_internal.idb_ptr)
        rt_db_free_internal(&region_internal);
    if (database != DBI_NULL)
        db_close(database);
    return result;
}


static int
read_manifest(const char *path, std::string *contents)
{
    std::ifstream input(path, std::ios::binary);

    if (!input)
        return 0;

    *contents = std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return 1;
}


int
main(int argument_count, char *arguments[])
{
    if (argument_count != 3)
        return 1;

    const std::string working_directory = arguments[1];
    const std::string snapshot_path = path_join(working_directory, FIXTURE_SNAPSHOT_NAME);
    const std::string malformed_snapshot_path = path_join(working_directory, MALFORMED_SNAPSHOT_NAME);
    const std::string unsupported_snapshot_path = path_join(working_directory, UNSUPPORTED_SNAPSHOT_NAME);
    const std::string output_path = path_join(working_directory, OUTPUT_DATABASE_NAME);
    std::string expected_manifest;
    std::string actual_manifest;
    int result = 1;

    std::remove(snapshot_path.c_str());
    std::remove(malformed_snapshot_path.c_str());
    std::remove(unsupported_snapshot_path.c_str());
    std::remove(output_path.c_str());

    if (!write_snapshot_fixture(snapshot_path, output_path, CREO_BRL_SNAPSHOT_FORMAT_VERSION_V2) ||
        creo_brl_core_convert(snapshot_path.c_str()) != CREO_BRL_CORE_CONVERT_SUCCESS ||
        !normalize_output_manifest(output_path, &actual_manifest) ||
        !read_manifest(arguments[2], &expected_manifest) || actual_manifest != expected_manifest ||
        !write_malformed_snapshot(malformed_snapshot_path) ||
        creo_brl_core_convert(malformed_snapshot_path.c_str()) != CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT ||
        !write_snapshot_fixture(unsupported_snapshot_path, output_path,
                                CREO_BRL_SNAPSHOT_FORMAT_VERSION_V2 + 1u) ||
        creo_brl_core_convert(unsupported_snapshot_path.c_str()) !=
            CREO_BRL_CORE_CONVERT_UNSUPPORTED_SNAPSHOT ||
        creo_brl_core_convert(NULL) != CREO_BRL_CORE_CONVERT_INVALID_REQUEST ||
        creo_brl_core_convert(EMPTY_REQUEST) != CREO_BRL_CORE_CONVERT_INVALID_REQUEST) {
        test_failure("snapshot core boundary test failed");
        goto cleanup;
    }

    result = 0;

cleanup:
    std::remove(snapshot_path.c_str());
    std::remove(malformed_snapshot_path.c_str());
    std::remove(unsupported_snapshot_path.c_str());
    std::remove(output_path.c_str());
    return result;
}
