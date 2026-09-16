#include "common.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "wdb.h"
#include "bu/avs.h"
#include "conversion_snapshot.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/nongeom.h"
#include "rt/tree.h"
#include "shared/core_api.h"


static const char FIXTURE_MODEL_NAME[] = "fixture_part";
static const char FIXTURE_MODEL_VERSION[] = "phase3";
static const char FIXTURE_MATERIAL_NAME[] = "fixture_material";
static const char FIXTURE_SOLID_NAME[] = "fixture_part.s";
static const char FIXTURE_FALLBACK_NAME[] = "fixture_part.rpp";
static const char FIXTURE_REGION_NAME[] = "fixture_part.r";
static const char FIXTURE_TOP_LEVEL_NAME[] = "all";
static const char MALFORMED_SNAPSHOT_NAME[] = "creo_phase3_malformed.snapshot";
static const char INVALID_SIZE_SNAPSHOT_NAME[] = "creo_phase3_invalid_size.snapshot";
static const char INVALID_TRIANGLE_SNAPSHOT_NAME[] = "creo_phase3_invalid_triangle.snapshot";
static const char EMPTY_REQUEST[] = "";
static const char IMPORTER_ATTRIBUTE[] = "importer";
static const char MODEL_NAME_ATTRIBUTE[] = "ptc_name";
static const char MODEL_VERSION_ATTRIBUTE[] = "ptc_version_stamp";
static const char MATERIAL_ATTRIBUTE[] = "ptc_material_name";
static const char FALLBACK_ATTRIBUTE[] = "tess_fail";
static const char IMPORTER_VALUE[] = "creo-g";
static const unsigned char FIXTURE_COLOR[] = {17u, 34u, 51u};
static const char MANIFEST_LINE_ENDING = 10;
static const uint64_t VALID_SNAPSHOT_SIZE_DELTA = 0u;
static const uint64_t INVALID_SNAPSHOT_SIZE_DELTA = 1u;
static const int FIXTURE_REGION_ID = 42;


struct fixture_definition {
    const char *fixture_name;
    const char *snapshot_name;
    const char *output_name;
    const char *log_name;
    const char *manifest_name;
    int expected_result;
    int compare_manifest;
    struct creo_brl_snapshot_settings settings;
    struct creo_brl_snapshot_part part;
    std::string rename_parameters;
    std::string preserve_parameters;
    std::vector<std::pair<std::string, std::string> > attributes;
    std::vector<std::pair<std::string, std::string> > parameters;
    std::vector<struct creo_brl_snapshot_vector> vertices;
    std::vector<struct creo_brl_snapshot_triangle> triangles;
    std::vector<struct creo_brl_snapshot_vector> normals;
    std::vector<uint32_t> normal_indices;
};


static int
test_failure(const char *message)
{
    std::fprintf(stderr, "%s\n", message);
    return 0;
}


static int
test_failuref(const char *fixture_name, const char *message)
{
    std::fprintf(stderr, "%s: %s\n", fixture_name, message);
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
               size_t record_size);


static int
append_named_values(std::vector<unsigned char> *contents,
                    const std::vector<std::pair<std::string, std::string> >& values,
                    struct creo_brl_snapshot_range *range,
                    uint64_t *count)
{
    const size_t records_offset = contents->size();
    std::vector<struct creo_brl_snapshot_named_value> records(values.size());

    if (!contents || !range || !count)
        return 0;

    *count = values.size();
    if (values.empty()) {
        range->offset = 0;
        range->size = 0;
        return 1;
    }

    if (!append_data(contents, records.data(), values.size() * sizeof(records[0]), range))
        return 0;

    for (size_t index = 0; index < values.size(); ++index) {
        if (!append_data(contents, values[index].first.data(), values[index].first.size(),
                         &records[index].name) ||
            !append_data(contents, values[index].second.data(), values[index].second.size(),
                         &records[index].value) ||
            !replace_record(contents, records_offset + index * sizeof(records[index]),
                            &records[index], sizeof(records[index])))
            return 0;
    }

    return 1;
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


static void
set_bbox(struct creo_brl_snapshot_part *part,
         double min_x, double min_y, double min_z,
         double max_x, double max_y, double max_z)
{
    const double dx = max_x - min_x;
    const double dy = max_y - min_y;
    const double dz = max_z - min_z;

    part->bbox_min.coordinates[0] = min_x;
    part->bbox_min.coordinates[1] = min_y;
    part->bbox_min.coordinates[2] = min_z;
    part->bbox_max.coordinates[0] = max_x;
    part->bbox_max.coordinates[1] = max_y;
    part->bbox_max.coordinates[2] = max_z;
    part->bbox_area = 2.0 * (dx * dy + dx * dz + dy * dz);
    part->bbox_diagonal = std::sqrt(dx * dx + dy * dy + dz * dz);
    part->bbox_volume = dx * dy * dz;
}


static fixture_definition
make_mesh_fixture(const char *fixture_name,
                  const char *snapshot_name,
                  const char *output_name,
                  const char *manifest_name,
                  uint32_t settings_flags,
                  const std::vector<struct creo_brl_snapshot_vector>& vertices,
                  const std::vector<struct creo_brl_snapshot_triangle>& triangles,
                  const std::vector<struct creo_brl_snapshot_vector>& normals,
                  const std::vector<uint32_t>& normal_indices,
                  double max_x,
                  double max_y,
                  double max_z)
{
    fixture_definition fixture = {};

    fixture.fixture_name = fixture_name;
    fixture.snapshot_name = snapshot_name;
    fixture.output_name = output_name;
    fixture.log_name = NULL;
    fixture.manifest_name = manifest_name;
    fixture.expected_result = CREO_BRL_CORE_CONVERT_SUCCESS;
    fixture.compare_manifest = 1;
    fixture.settings.flags = settings_flags;
    fixture.settings.log_type = CREO_BRL_SNAPSHOT_LOG_NONE;
    fixture.part.flags = CREO_BRL_SNAPSHOT_PART_HAS_MESH |
                         CREO_BRL_SNAPSHOT_PART_HAS_COLOR |
                         CREO_BRL_SNAPSHOT_PART_HAS_MATERIAL;
    if (!normals.empty() && !normal_indices.empty())
        fixture.part.flags |= CREO_BRL_SNAPSHOT_PART_HAS_NORMALS;
    fixture.part.region_id = FIXTURE_REGION_ID;
    fixture.part.model_to_mm = 1.0;
    fixture.part.color_is_set = 1;
    std::memcpy(fixture.part.color, FIXTURE_COLOR, sizeof(fixture.part.color));
    set_bbox(&fixture.part, 0.0, 0.0, 0.0, max_x, max_y, max_z);
    fixture.vertices = vertices;
    fixture.triangles = triangles;
    fixture.normals = normals;
    fixture.normal_indices = normal_indices;
    return fixture;
}


static fixture_definition
make_bbox_fallback_fixture(const char *fixture_name,
                           const char *snapshot_name,
                           const char *output_name,
                           const char *manifest_name,
                           uint32_t settings_flags)
{
    fixture_definition fixture = {};

    fixture.fixture_name = fixture_name;
    fixture.snapshot_name = snapshot_name;
    fixture.output_name = output_name;
    fixture.log_name = NULL;
    fixture.manifest_name = manifest_name;
    fixture.expected_result = CREO_BRL_CORE_CONVERT_SUCCESS;
    fixture.compare_manifest = 1;
    fixture.settings.flags = settings_flags;
    fixture.settings.log_type = CREO_BRL_SNAPSHOT_LOG_NONE;
    fixture.part.flags = CREO_BRL_SNAPSHOT_PART_HAS_BBOX_FALLBACK |
                         CREO_BRL_SNAPSHOT_PART_HAS_COLOR |
                         CREO_BRL_SNAPSHOT_PART_HAS_MATERIAL;
    fixture.part.region_id = FIXTURE_REGION_ID;
    fixture.part.model_to_mm = 1.0;
    fixture.part.color_is_set = 1;
    std::memcpy(fixture.part.color, FIXTURE_COLOR, sizeof(fixture.part.color));
    set_bbox(&fixture.part, 0.0, 0.0, 0.0, 100.0, 100.0, 10.0);
    return fixture;
}


static int
write_snapshot_fixture(const std::string& snapshot_path,
                       const std::string& output_path,
                       const std::string& log_path,
                       const fixture_definition& fixture,
                       uint64_t snapshot_size_adjustment)
{
    struct creo_brl_snapshot_header header = {};
    struct creo_brl_snapshot_scene scene = {};
    struct creo_brl_snapshot_part part = fixture.part;
    std::vector<unsigned char> contents;
    const size_t header_offset = 0;
    const size_t scene_offset = sizeof(header);
    const size_t part_offset = sizeof(header) + sizeof(scene);

    std::memcpy(header.magic, creo_brl_snapshot_magic, sizeof(header.magic));

    scene.settings = fixture.settings;
    scene.part_count = 1u;
    scene.parts.offset = part_offset;
    scene.parts.size = sizeof(part);
    scene.root_type = CREO_BRL_SNAPSHOT_SCENE_NODE_PART;

    if (!append_record(&contents, &header, sizeof(header)) ||
        !append_record(&contents, &scene, sizeof(scene)) ||
        !append_record(&contents, &part, sizeof(part)) ||
        !append_data(&contents, output_path.data(), output_path.size(), &scene.settings.output_path) ||
        !append_data(&contents, log_path.data(), log_path.size(), &scene.settings.log_path) ||
        !append_data(&contents, fixture.rename_parameters.data(), fixture.rename_parameters.size(),
                     &scene.settings.rename_parameters) ||
        !append_data(&contents, fixture.preserve_parameters.data(), fixture.preserve_parameters.size(),
                     &scene.settings.preserve_parameters) ||
        !append_data(&contents, FIXTURE_MODEL_NAME, std::strlen(FIXTURE_MODEL_NAME), &part.model_name) ||
        !append_data(&contents, FIXTURE_MODEL_VERSION, std::strlen(FIXTURE_MODEL_VERSION),
                     &part.model_version) ||
        !append_data(&contents, FIXTURE_MATERIAL_NAME, std::strlen(FIXTURE_MATERIAL_NAME),
                     &part.material_name) ||
        !append_data(&contents,
                     fixture.vertices.empty() ? NULL : fixture.vertices.data(),
                     fixture.vertices.size() * sizeof(fixture.vertices[0]), &part.vertices) ||
        !append_data(&contents,
                     fixture.triangles.empty() ? NULL : fixture.triangles.data(),
                     fixture.triangles.size() * sizeof(fixture.triangles[0]), &part.triangles) ||
        !append_data(&contents,
                     fixture.normals.empty() ? NULL : fixture.normals.data(),
                     fixture.normals.size() * sizeof(fixture.normals[0]), &part.normals) ||
        !append_data(&contents,
                     fixture.normal_indices.empty() ? NULL : fixture.normal_indices.data(),
                     fixture.normal_indices.size() * sizeof(fixture.normal_indices[0]),
                     &part.normal_indices) ||
        !append_named_values(&contents, fixture.attributes, &part.attributes, &part.attribute_count) ||
        !append_named_values(&contents, fixture.parameters, &part.parameters, &part.parameter_count))
        return 0;

    part.vertex_count = fixture.vertices.size();
    part.triangle_count = fixture.triangles.size();
    part.normal_count = fixture.normals.size();
    part.normal_index_count = fixture.normal_indices.size();
    if (contents.size() > UINT64_MAX - snapshot_size_adjustment)
        return 0;
    header.snapshot_size = (uint64_t)contents.size() + snapshot_size_adjustment;

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

    if (directory == RT_DIR_NULL ||
        db5_get_attributes(database, &attributes, directory) != 0)
        goto cleanup;

    attribute_value = bu_avs_get(&attributes, attribute_name);
    if (!attribute_value)
        goto cleanup;

    *value = attribute_value;
    result = 1;

cleanup:
    bu_avs_free(&attributes);
    return result;
}


static int
object_exists(struct db_i *database, const char *object_name)
{
    return db_lookup(database, object_name, LOOKUP_QUIET) != RT_DIR_NULL;
}


static int
write_empty_database(const std::string& output_path)
{
    struct db_i *database = db_create(output_path.c_str(), BRLCAD_DB_FORMAT_LATEST);

    if (database == DBI_NULL)
        return 0;

    db_close(database);
    return 1;
}


static int
write_collision_database(const std::string& output_path)
{
    struct db_i *database = db_create(output_path.c_str(), BRLCAD_DB_FORMAT_LATEST);
    struct rt_wdb *writer = RT_WDB_NULL;
    struct wmember members;
    point_t min_point = {0.0, 0.0, 0.0};
    point_t max_point = {1.0, 1.0, 1.0};
    int result = 0;

    if (database == DBI_NULL)
        return 0;
    writer = wdb_dbopen(database, RT_WDB_TYPE_DB_DISK);
    if (writer == RT_WDB_NULL)
        goto cleanup;

    if (mk_rpp(writer, "widget_7.s", min_point, max_point) != 0)
        goto cleanup;
    BU_LIST_INIT(&members.l);
    if (mk_addmember("widget_7.s", &members.l, NULL, WMOP_UNION) == WMEMBER_NULL)
        goto cleanup;
    if (mk_comb(writer, "widget_7.r", &members.l, 1, NULL, NULL, NULL, 1, 0, 0, 0, 0, 0, 0) != 0)
        goto cleanup;
    BU_LIST_INIT(&members.l);
    if (mk_addmember("widget_7.r", &members.l, NULL, WMOP_UNION) == WMEMBER_NULL)
        goto cleanup;
    if (mk_lcomb(writer, "all", &members, 0, NULL, NULL, NULL, 0) != 0)
        goto cleanup;

    result = 1;

cleanup:
    if (database != DBI_NULL)
        db_close(database);
    return result;
}


static int
read_text_file(const std::string& path, std::string *contents)
{
    std::ifstream input(path.c_str(), std::ios::binary);

    if (!input)
        return 0;

    *contents = std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return 1;
}


static int
expect_phase4_attribute(struct db_i *database,
                        const char *object_name,
                        const char *attribute_name,
                        const char *expected_value,
                        const char *fixture_name)
{
    std::string value;

    if (!read_attribute(database, object_name, attribute_name, &value)) {
        std::fprintf(stderr, "%s: missing attribute %s on %s\n",
                     fixture_name, attribute_name, object_name);
        return 0;
    }
    if (value != expected_value) {
        std::fprintf(stderr, "%s: attribute %s on %s expected '%s' got '%s'\n",
                     fixture_name, attribute_name, object_name, expected_value, value.c_str());
        return 0;
    }

    return 1;
}


static int
verify_phase4_metadata(const std::string& output_path,
                       const char *fixture_name,
                       const char *region_name,
                       const char *solid_name,
                       const char *top_name,
                       int expect_fallback)
{
    struct db_i *database = db_open(output_path.c_str(), DB_OPEN_READONLY);
    std::string value;
    int result = 0;

    if (database == DBI_NULL || db_dirbuild(database) < 0)
        goto cleanup;

    if (!object_exists(database, region_name)) {
        std::fprintf(stderr, "%s: missing region %s\n", fixture_name, region_name);
        goto cleanup;
    }
    if (!object_exists(database, solid_name)) {
        std::fprintf(stderr, "%s: missing solid %s\n", fixture_name, solid_name);
        goto cleanup;
    }
    if (!object_exists(database, top_name)) {
        std::fprintf(stderr, "%s: missing top object %s\n", fixture_name, top_name);
        goto cleanup;
    }

    result = expect_phase4_attribute(database, region_name, MATERIAL_ATTRIBUTE, FIXTURE_MATERIAL_NAME,
                                     fixture_name) &&
             expect_phase4_attribute(database, region_name, "density", "1.25", fixture_name) &&
             expect_phase4_attribute(database, region_name, "mass", "2.5", fixture_name) &&
             expect_phase4_attribute(database, region_name, "volume", "3.75", fixture_name) &&
             expect_phase4_attribute(database, region_name, "surface_area", "4.5", fixture_name) &&
             expect_phase4_attribute(database, region_name, "custom_attr", "custom_value", fixture_name) &&
             expect_phase4_attribute(database, region_name, "nomenclature", "Widget 7", fixture_name) &&
             expect_phase4_attribute(database, region_name, "part_number", "PN-42", fixture_name);
    if (!result)
        goto cleanup;

    if (expect_fallback)
        result = read_attribute(database, solid_name, FALLBACK_ATTRIBUTE, &value) && value == FIXTURE_MODEL_NAME;
    else
        result = !read_attribute(database, solid_name, FALLBACK_ATTRIBUTE, &value);

cleanup:
    if (database != DBI_NULL)
        db_close(database);
    return result;
}


static int
verify_phase4_failed_output(const std::string& output_path)
{
    struct db_i *database = db_open(output_path.c_str(), DB_OPEN_READONLY);
    int result = 1;

    if (database == DBI_NULL)
        return 1;
    if (db_dirbuild(database) < 0) {
        result = 0;
        goto cleanup;
    }

    result = !object_exists(database, FIXTURE_REGION_NAME) &&
             !object_exists(database, FIXTURE_SOLID_NAME) &&
             !object_exists(database, FIXTURE_TOP_LEVEL_NAME);

cleanup:
    db_close(database);
    return result;
}


static int
run_phase4_log_fixture(const std::string& working_directory,
                       const fixture_definition& fixture,
                       const char *expected_log)
{
    const std::string snapshot_path = path_join(working_directory, fixture.snapshot_name);
    const std::string output_path = path_join(working_directory, fixture.output_name);
    const std::string log_path = fixture.log_name ? path_join(working_directory, fixture.log_name) : std::string();
    std::string log_contents;
    int result = 0;

    std::remove(snapshot_path.c_str());
    std::remove(output_path.c_str());
    if (!log_path.empty())
        std::remove(log_path.c_str());
    if (!write_snapshot_fixture(snapshot_path, output_path, log_path, fixture, VALID_SNAPSHOT_SIZE_DELTA) ||
        creo_brl_core_convert(snapshot_path.c_str()) != fixture.expected_result ||
        log_path.empty() || !read_text_file(log_path, &log_contents) ||
        log_contents.find(expected_log) == std::string::npos ||
        (fixture.expected_result != CREO_BRL_CORE_CONVERT_SUCCESS &&
         !verify_phase4_failed_output(output_path))) {
        test_failuref(fixture.fixture_name, "phase 4 log verification failed");
        goto cleanup;
    }

    result = 1;

cleanup:
    std::remove(snapshot_path.c_str());
    std::remove(output_path.c_str());
    if (!log_path.empty())
        std::remove(log_path.c_str());
    return result;
}


static int
run_phase4_fixture(const std::string& working_directory,
                   const fixture_definition& fixture,
                   int precreate_output,
                   const char *expected_region_name,
                   const char *expected_solid_name,
                   const char *expected_top_name,
                   int expect_fallback)
{
    const std::string snapshot_path = path_join(working_directory, fixture.snapshot_name);
    const std::string output_path = path_join(working_directory, fixture.output_name);
    const std::string log_path = fixture.log_name ? path_join(working_directory, fixture.log_name) : std::string();
    int result = 0;

    std::remove(snapshot_path.c_str());
    std::remove(output_path.c_str());
    if (!log_path.empty())
        std::remove(log_path.c_str());
    if (precreate_output && !write_collision_database(output_path))
        return test_failuref(fixture.fixture_name, "failed to precreate output database");
    if (!write_snapshot_fixture(snapshot_path, output_path, log_path, fixture, VALID_SNAPSHOT_SIZE_DELTA) ||
        creo_brl_core_convert(snapshot_path.c_str()) != CREO_BRL_CORE_CONVERT_SUCCESS ||
        !verify_phase4_metadata(output_path, fixture.fixture_name,
                                expected_region_name, expected_solid_name,
                                expected_top_name, expect_fallback)) {
        test_failuref(fixture.fixture_name, "phase 4 verification failed");
        goto cleanup;
    }

    result = 1;

cleanup:
    std::remove(snapshot_path.c_str());
    std::remove(output_path.c_str());
    if (!log_path.empty())
        std::remove(log_path.c_str());
    return result;
}


static std::string
format_double(double value)
{
    std::ostringstream stream;
    std::string text;

    stream << std::setprecision(15) << value;
    text = stream.str();
    if (text.find('.') != std::string::npos) {
        while (!text.empty() && text[text.size() - 1] == '0')
            text.resize(text.size() - 1);
        if (!text.empty() && text[text.size() - 1] == '.')
            text.resize(text.size() - 1);
    }
    return text;
}


static int
normalize_output_manifest(const char *fixture_name,
                          const std::string& output_path,
                          std::string *manifest)
{
    struct db_i *database = db_open(output_path.c_str(), DB_OPEN_READONLY);
    struct directory *solid_directory = RT_DIR_NULL;
    struct directory *region_directory = RT_DIR_NULL;
    struct rt_db_internal solid_internal = RT_DB_INTERNAL_INIT_ZERO;
    struct rt_db_internal region_internal = RT_DB_INTERNAL_INIT_ZERO;
    std::string solid_name;
    std::string solid_importer;
    std::string solid_model_name;
    std::string solid_fallback;
    std::string region_model_name;
    std::string region_model_version;
    std::string region_material_name;
    int result = 0;

    if (database == DBI_NULL || db_dirbuild(database) < 0)
        goto cleanup;

    solid_directory = db_lookup(database, FIXTURE_SOLID_NAME, LOOKUP_QUIET);
    if (solid_directory != RT_DIR_NULL)
        solid_name = FIXTURE_SOLID_NAME;
    else {
        solid_directory = db_lookup(database, FIXTURE_FALLBACK_NAME, LOOKUP_QUIET);
        if (solid_directory != RT_DIR_NULL)
            solid_name = FIXTURE_FALLBACK_NAME;
    }
    region_directory = db_lookup(database, FIXTURE_REGION_NAME, LOOKUP_QUIET);
    if (solid_directory == RT_DIR_NULL || region_directory == RT_DIR_NULL ||
        rt_db_get_internal(&solid_internal, solid_directory, database, NULL) < 0 ||
        rt_db_get_internal(&region_internal, region_directory, database, NULL) < 0 ||
        region_internal.idb_type != ID_COMBINATION ||
        !combination_has_leaf(database, FIXTURE_TOP_LEVEL_NAME, FIXTURE_REGION_NAME) ||
        !combination_has_leaf(database, FIXTURE_REGION_NAME, solid_name.c_str()) ||
        !read_attribute(database, solid_name.c_str(), IMPORTER_ATTRIBUTE, &solid_importer) ||
        !read_attribute(database, solid_name.c_str(), MODEL_NAME_ATTRIBUTE, &solid_model_name) ||
        !read_attribute(database, FIXTURE_REGION_NAME, MODEL_NAME_ATTRIBUTE, &region_model_name) ||
        !read_attribute(database, FIXTURE_REGION_NAME, MODEL_VERSION_ATTRIBUTE, &region_model_version) ||
        !read_attribute(database, FIXTURE_REGION_NAME, MATERIAL_ATTRIBUTE, &region_material_name) ||
        solid_importer != IMPORTER_VALUE)
        goto cleanup;

    {
        struct rt_comb_internal *region =
            static_cast<struct rt_comb_internal *>(region_internal.idb_ptr);

        if (!region->region_flag || region->region_id != FIXTURE_REGION_ID || !region->rgb_valid ||
            std::memcmp(region->rgb, FIXTURE_COLOR, sizeof(region->rgb)) != 0)
            goto cleanup;

        manifest->append("fixture=");
        manifest->append(fixture_name);
        manifest->push_back(MANIFEST_LINE_ENDING);
        manifest->append("input=synthetic_scene");
        manifest->push_back(MANIFEST_LINE_ENDING);
        manifest->append("all=combination leaf=");
        manifest->append(FIXTURE_REGION_NAME);
        manifest->push_back(MANIFEST_LINE_ENDING);

        if (solid_internal.idb_type == ID_BOT) {
            struct rt_bot_internal *bot = static_cast<struct rt_bot_internal *>(solid_internal.idb_ptr);

            manifest->append(solid_name);
            manifest->append("=bot vertices=");
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
        } else if (solid_internal.idb_type == ID_ARB8) {
            struct rt_arb_internal *arb = static_cast<struct rt_arb_internal *>(solid_internal.idb_ptr);
            point_t min_point, max_point;

            VMOVE(min_point, arb->pt[0]);
            VMOVE(max_point, arb->pt[0]);
            for (size_t point_index = 1; point_index < 8; ++point_index) {
                for (size_t coordinate = 0; coordinate < 3; ++coordinate) {
                    if (arb->pt[point_index][coordinate] < min_point[coordinate])
                        min_point[coordinate] = arb->pt[point_index][coordinate];
                    if (arb->pt[point_index][coordinate] > max_point[coordinate])
                        max_point[coordinate] = arb->pt[point_index][coordinate];
                }
            }
            if (!read_attribute(database, solid_name.c_str(), FALLBACK_ATTRIBUTE, &solid_fallback))
                goto cleanup;

            manifest->append(solid_name);
            manifest->append("=rpp min=");
            manifest->append(format_double(min_point[0]));
            manifest->push_back(',');
            manifest->append(format_double(min_point[1]));
            manifest->push_back(',');
            manifest->append(format_double(min_point[2]));
            manifest->append(" max=");
            manifest->append(format_double(max_point[0]));
            manifest->push_back(',');
            manifest->append(format_double(max_point[1]));
            manifest->push_back(',');
            manifest->append(format_double(max_point[2]));
            manifest->append(" importer=");
            manifest->append(solid_importer);
            manifest->append(" ptc_name=");
            manifest->append(solid_model_name);
            manifest->append(" tess_fail=");
            manifest->append(solid_fallback);
            manifest->push_back(MANIFEST_LINE_ENDING);
        } else {
            goto cleanup;
        }

        manifest->append(FIXTURE_REGION_NAME);
        manifest->append("=region id=");
        manifest->append(std::to_string(region->region_id));
        manifest->append(" rgb=");
        manifest->append(std::to_string((unsigned int)region->rgb[0]));
        manifest->push_back(',');
        manifest->append(std::to_string((unsigned int)region->rgb[1]));
        manifest->push_back(',');
        manifest->append(std::to_string((unsigned int)region->rgb[2]));
        manifest->append(" leaf=");
        manifest->append(solid_name);
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
    contents->erase(std::remove(contents->begin(), contents->end(), '\r'), contents->end());
    return 1;
}


static int
run_fixture(const std::string& working_directory,
            const std::string& fixtures_directory,
            const fixture_definition& fixture)
{
    const std::string snapshot_path = path_join(working_directory, fixture.snapshot_name);
    const std::string output_path = path_join(working_directory, fixture.output_name);
    const std::string log_path = fixture.log_name ? path_join(working_directory, fixture.log_name) : std::string();
    const std::string manifest_path = fixture.manifest_name ?
                                          path_join(fixtures_directory, fixture.manifest_name) :
                                          std::string();
    std::string expected_manifest;
    std::string actual_manifest;
    int result = 0;

    std::remove(snapshot_path.c_str());
    std::remove(output_path.c_str());
    if (!log_path.empty())
        std::remove(log_path.c_str());

    if (!write_snapshot_fixture(snapshot_path, output_path, log_path, fixture, VALID_SNAPSHOT_SIZE_DELTA)) {
        test_failuref(fixture.fixture_name, "failed to write snapshot fixture");
        goto cleanup;
    }

    if (creo_brl_core_convert(snapshot_path.c_str()) != fixture.expected_result) {
        test_failuref(fixture.fixture_name, "unexpected conversion result");
        goto cleanup;
    }

    if (fixture.compare_manifest) {
        if (!normalize_output_manifest(fixture.fixture_name, output_path, &actual_manifest)) {
            test_failuref(fixture.fixture_name, "failed to normalize output manifest");
            goto cleanup;
        }
        if (!read_manifest(manifest_path.c_str(), &expected_manifest)) {
            test_failuref(fixture.fixture_name, "failed to read expected manifest");
            goto cleanup;
        }
        if (actual_manifest != expected_manifest) {
            std::fprintf(stderr, "%s: manifest mismatch\nEXPECTED:\n%sACTUAL:\n%s",
                         fixture.fixture_name, expected_manifest.c_str(), actual_manifest.c_str());
            goto cleanup;
        }
    }

    result = 1;

cleanup:
    std::remove(snapshot_path.c_str());
    std::remove(output_path.c_str());
    if (!log_path.empty())
        std::remove(log_path.c_str());
    return result;
}


int
main(int argument_count, char *arguments[])
{
    if (argument_count != 3)
        return 1;

    const std::string working_directory = arguments[1];
    const std::string fixtures_directory = arguments[2];
    const std::string malformed_snapshot_path = path_join(working_directory, MALFORMED_SNAPSHOT_NAME);
    const std::string invalid_size_snapshot_path = path_join(working_directory, INVALID_SIZE_SNAPSHOT_NAME);
    const std::string invalid_triangle_snapshot_path = path_join(working_directory, INVALID_TRIANGLE_SNAPSHOT_NAME);
    const std::string invalid_triangle_output_path = path_join(working_directory, "creo_phase3_invalid_triangle.g");
    int result = 1;

    const fixture_definition normalized_mesh_fixture = make_mesh_fixture(
        "scene_normalized_mesh",
        "creo_phase3_normalized_mesh.snapshot",
        "creo_phase3_normalized_mesh.g",
        "scene_normalized_mesh.manifest",
        0u,
        {{{0.0, 0.0, 0.0}}, {{100.0, 0.0, 0.0}}, {{100.0, 100.0, 0.0}},
         {{0.0, 0.0, 0.0}}, {{100.0, 100.0, 0.0}}, {{0.0, 100.0, 0.0}}},
        {{{0u, 1u, 2u}}, {{3u, 4u, 5u}}}, {}, {}, 100.0, 100.0, 0.0);
    const fixture_definition normals_fixture = make_mesh_fixture(
        "scene_mesh_with_normals",
        "creo_phase3_normals.snapshot",
        "creo_phase3_normals.g",
        "scene_mesh_with_normals.manifest",
        CREO_BRL_SNAPSHOT_SETTING_WRITE_NORMALS,
        {{{0.0, 0.0, 0.0}}, {{100.0, 0.0, 0.0}}, {{0.0, 100.0, 0.0}}},
        {{{0u, 1u, 2u}}},
        {{{0.0, 0.0, 1.0}}, {{0.0, 0.0, 1.0}}, {{0.0, 0.0, 1.0}}},
        {0u, 1u, 2u}, 100.0, 100.0, 0.0);
    const fixture_definition solid_fixture = make_mesh_fixture(
        "scene_solid_mesh",
        "creo_phase3_solid.snapshot",
        "creo_phase3_solid.g",
        "scene_solid_mesh.manifest",
        CREO_BRL_SNAPSHOT_SETTING_CHECK_SOLIDITY,
        {{{0.0, 0.0, 0.0}}, {{0.0, 100.0, 0.0}}, {{100.0, 0.0, 0.0}},
         {{0.0, 0.0, 0.0}}, {{100.0, 0.0, 0.0}}, {{0.0, 0.0, 100.0}},
         {{0.0, 0.0, 0.0}}, {{0.0, 0.0, 100.0}}, {{0.0, 100.0, 0.0}},
         {{100.0, 0.0, 0.0}}, {{0.0, 100.0, 0.0}}, {{0.0, 0.0, 100.0}}},
        {{{0u, 1u, 2u}}, {{3u, 4u, 5u}}, {{6u, 7u, 8u}}, {{9u, 10u, 11u}}},
        {}, {}, 100.0, 100.0, 100.0);
    const fixture_definition fallback_fixture = make_mesh_fixture(
        "scene_box_fallback",
        "creo_phase3_box_fallback.snapshot",
        "creo_phase3_box_fallback.g",
        "scene_box_fallback.manifest",
        CREO_BRL_SNAPSHOT_SETTING_CHECK_SOLIDITY | CREO_BRL_SNAPSHOT_SETTING_CREATE_BOXES,
        {{{0.0, 0.0, 0.0}}, {{100.0, 0.0, 0.0}}, {{0.0, 100.0, 0.0}}},
        {{{0u, 1u, 2u}}}, {}, {}, 100.0, 100.0, 10.0);
    const fixture_definition bbox_fixture = make_bbox_fallback_fixture(
        "scene_bbox_fallback",
        "creo_phase3_bbox_fallback.snapshot",
        "creo_phase3_bbox_fallback.g",
        "scene_bbox_fallback.manifest",
        0u);
    fixture_definition failure_fixture = make_mesh_fixture(
        "scene_output_failure",
        "creo_phase3_output_failure.snapshot",
        "creo_phase3_output_failure.g",
        NULL,
        CREO_BRL_SNAPSHOT_SETTING_CHECK_SOLIDITY,
        {{{0.0, 0.0, 0.0}}, {{100.0, 0.0, 0.0}}, {{0.0, 100.0, 0.0}}},
        {{{0u, 1u, 2u}}}, {}, {}, 100.0, 100.0, 10.0);
    fixture_definition invalid_triangle_fixture = make_mesh_fixture(
        "scene_invalid_triangle",
        INVALID_TRIANGLE_SNAPSHOT_NAME,
        "creo_phase3_invalid_triangle.g",
        NULL,
        0u,
        {{{0.0, 0.0, 0.0}}, {{100.0, 0.0, 0.0}}, {{0.0, 100.0, 0.0}}},
        {{{0u, 1u, 9u}}}, {}, {}, 100.0, 100.0, 0.0);
    failure_fixture.expected_result = CREO_BRL_CORE_CONVERT_OUTPUT_FAILED;
    failure_fixture.compare_manifest = 0;
    failure_fixture.log_name = "creo_phase4_output_failure.log";
    failure_fixture.settings.log_type = CREO_BRL_SNAPSHOT_LOG_FAILURE;
    failure_fixture.part.flags |= CREO_BRL_SNAPSHOT_PART_HAS_MASS_PROPERTIES;
    failure_fixture.part.density = 1.25;
    failure_fixture.part.mass = 2.5;
    failure_fixture.part.volume = 3.75;
    failure_fixture.part.surface_area = 4.5;
    failure_fixture.rename_parameters = "nomenclature";
    failure_fixture.preserve_parameters = "part_number";
    failure_fixture.attributes.push_back(std::make_pair(std::string("custom_attr"), std::string("custom_value")));
    failure_fixture.parameters.push_back(std::make_pair(std::string("nomenclature"), std::string("Widget 7")));
    failure_fixture.parameters.push_back(std::make_pair(std::string("part_number"), std::string("PN-42")));
    invalid_triangle_fixture.expected_result = CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT;
    invalid_triangle_fixture.compare_manifest = 0;
    fixture_definition metadata_fixture = make_mesh_fixture(
        "scene_phase4_metadata",
        "creo_phase4_metadata.snapshot",
        "creo_phase4_metadata.g",
        NULL,
        0u,
        {{{0.0, 0.0, 0.0}}, {{100.0, 0.0, 0.0}}, {{100.0, 100.0, 0.0}},
         {{0.0, 0.0, 0.0}}, {{100.0, 100.0, 0.0}}, {{0.0, 100.0, 0.0}}},
        {{{0u, 1u, 2u}}, {{3u, 4u, 5u}}}, {}, {}, 100.0, 100.0, 0.0);
    fixture_definition fallback_metadata_fixture = make_bbox_fallback_fixture(
        "scene_phase4_fallback_metadata",
        "creo_phase4_fallback.snapshot",
        "creo_phase4_fallback.g",
        NULL,
        0u);
    metadata_fixture.part.flags |= CREO_BRL_SNAPSHOT_PART_HAS_MASS_PROPERTIES;
    metadata_fixture.part.density = 1.25;
    metadata_fixture.part.mass = 2.5;
    metadata_fixture.part.volume = 3.75;
    metadata_fixture.part.surface_area = 4.5;
    metadata_fixture.rename_parameters = "nomenclature";
    metadata_fixture.preserve_parameters = "part_number";
    metadata_fixture.attributes.push_back(std::make_pair(std::string("custom_attr"), std::string("custom_value")));
    metadata_fixture.parameters.push_back(std::make_pair(std::string("nomenclature"), std::string("Widget 7")));
    metadata_fixture.parameters.push_back(std::make_pair(std::string("part_number"), std::string("PN-42")));
    fallback_metadata_fixture.part.flags |= CREO_BRL_SNAPSHOT_PART_HAS_MASS_PROPERTIES;
    fallback_metadata_fixture.part.density = 1.25;
    fallback_metadata_fixture.part.mass = 2.5;
    fallback_metadata_fixture.part.volume = 3.75;
    fallback_metadata_fixture.part.surface_area = 4.5;
    fallback_metadata_fixture.rename_parameters = "nomenclature";
    fallback_metadata_fixture.preserve_parameters = "part_number";
    fallback_metadata_fixture.attributes.push_back(std::make_pair(std::string("custom_attr"), std::string("custom_value")));
    fallback_metadata_fixture.parameters.push_back(std::make_pair(std::string("nomenclature"), std::string("Widget 7")));
    fallback_metadata_fixture.parameters.push_back(std::make_pair(std::string("part_number"), std::string("PN-42")));
    fixture_definition success_log_fixture = metadata_fixture;
    fixture_definition fallback_log_fixture = fallback_metadata_fixture;
    success_log_fixture.log_name = "creo_phase4_success.log";
    success_log_fixture.settings.log_type = CREO_BRL_SNAPSHOT_LOG_SUCCESS;
    fallback_log_fixture.log_name = "creo_phase4_fallback.log";
    fallback_log_fixture.settings.log_type = CREO_BRL_SNAPSHOT_LOG_FAILURE;

    std::remove(malformed_snapshot_path.c_str());
    std::remove(invalid_size_snapshot_path.c_str());
    std::remove(invalid_triangle_snapshot_path.c_str());
    std::remove(invalid_triangle_output_path.c_str());

    if (!run_fixture(working_directory, fixtures_directory, normalized_mesh_fixture) ||
        !run_fixture(working_directory, fixtures_directory, normals_fixture) ||
        !run_fixture(working_directory, fixtures_directory, solid_fixture) ||
        !run_fixture(working_directory, fixtures_directory, fallback_fixture) ||
        !run_fixture(working_directory, fixtures_directory, bbox_fixture) ||
        !run_phase4_fixture(working_directory, metadata_fixture, 0,
                           "widget_7.r", "widget_7.s", "all", 0) ||
        !run_phase4_fixture(working_directory, fallback_metadata_fixture, 0,
                           "widget_7.r", "widget_7.rpp", "all", 1) ||
        !run_phase4_fixture(working_directory, metadata_fixture, 1,
                           "widget_7_1.r", "widget_7_1.s", "all_1", 0) ||
        !run_phase4_log_fixture(working_directory, success_log_fixture,
                                "SUCCESS: Part \"fixture_part\" converted") ||
        !run_phase4_log_fixture(working_directory, fallback_log_fixture,
                                "FAILURE: Part \"fixture_part\" replaced with bounding box") ||
        !run_phase4_log_fixture(working_directory, failure_fixture,
                                "FAILURE: Part \"fixture_part\" not converted") ||
        !run_fixture(working_directory, fixtures_directory, invalid_triangle_fixture) ||
        !write_malformed_snapshot(malformed_snapshot_path) ||
        creo_brl_core_convert(malformed_snapshot_path.c_str()) != CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT ||
        !write_snapshot_fixture(invalid_size_snapshot_path, invalid_triangle_output_path, std::string(),
                                normalized_mesh_fixture, INVALID_SNAPSHOT_SIZE_DELTA) ||
        creo_brl_core_convert(invalid_size_snapshot_path.c_str()) !=
            CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT ||
        creo_brl_core_convert(NULL) != CREO_BRL_CORE_CONVERT_INVALID_REQUEST ||
        creo_brl_core_convert(EMPTY_REQUEST) != CREO_BRL_CORE_CONVERT_INVALID_REQUEST) {
        test_failure("snapshot core boundary test failed");
        goto cleanup;
    }

    result = 0;

cleanup:
    std::remove(malformed_snapshot_path.c_str());
    std::remove(invalid_size_snapshot_path.c_str());
    std::remove(invalid_triangle_snapshot_path.c_str());
    std::remove(invalid_triangle_output_path.c_str());
    return result;
}
