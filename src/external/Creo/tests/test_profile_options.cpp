#include "common.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <string>

extern "C" {
#include "raytrace.h"
#include "wdb.h"
}

#include "frontend/profile_path.h"
#include "frontend/profile_ui.h"
#include "shared/frontend_api.h"
#include "shared/conversion_options.h"
#include "shared/core_api.h"


static const int EXPECTED_ARGUMENT_COUNT = 3;
static const char OBSOLETE_STL_ATTRIBUTE[] = "export_facets_to_stl";
static const size_t PROFILE_PATH_BUFFER_SIZE = 1024;
static const size_t SMALL_PROFILE_PATH_BUFFER_SIZE = 4;
static const char PROFILE_TITLE[] = "Creo profile test";
static const char PROFILE_FILE_NAME[] = "profile_options.g";
static const char INVALID_PROFILE_FILE_NAME[] = "invalid_profile.g";
static const char INVALID_VALUE_PROFILE_FILE_NAME[] = "invalid_value_profile.g";
static const char BUNDLED_PROFILE_FILE_NAME[] = "bundled_profile.g";
static const char SELECTED_INVALID_PROFILE_FILE_NAME[] = "selected_invalid_profile.g";
static const char WORKING_PROFILE_FILE_NAME[] = "Creo_to_BRL.g";
static const char PROFILE_MATERIAL_DIRECTORY[] = "C:\\bundle\\text\\usascii";
static const char BUNDLED_MATERIAL_FILE[] = "C:\\bundle\\text\\usascii\\ptc_materials.mtl";
static const char LEGACY_MATERIAL_FILE[] = "./ptc_materials.mtl";
static const char DEFAULT_PATH_MODEL_NAME[] = "Widget.PRT.12";
static const char DEFAULT_OUTPUT_PATH[] = "widget.prt.g";
static const char DEFAULT_LOG_PATH[] = "widget.prt_log.txt";


struct profile_attribute {
    const char *name;
    const char *value;
};


struct observed_options {
    int received;
    int32_t log_type;
    int32_t xform_mode;
    int32_t chord_mode;
    uint32_t flags;
    std::string material_path;
    std::string rename_parameters;
    std::string preserve_parameters;
    std::string region_counter;
    std::string minimum_luminance;
    std::string maximum_chord;
    std::string minimum_angle;
    std::string minimum_hole;
    std::string minimum_chamfer;
    std::string minimum_round;
};


struct fake_controls {
    std::map<std::string, std::string> text;
    std::map<std::string, int> checks;
    std::map<std::string, std::string> radios;
};


static struct observed_options observed = {};
static std::string last_status;


static int
expect(int condition, const char *message)
{
    if (condition)
        return 1;

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
               directory + name : directory + "/" + name;
}


static std::string
read_file(const std::string& path)
{
    std::ifstream input(path.c_str(), std::ios::binary);

    return input ? std::string(std::istreambuf_iterator<char>(input),
                               std::istreambuf_iterator<char>()) : std::string();
}


static int
write_profile(const std::string& path, const struct profile_attribute *attributes, size_t attribute_count)
{
    struct rt_wdb *writer = wdb_fopen(path.c_str());
    int result = 0;

    if (writer == RT_WDB_NULL)
        return 0;
    if (mk_id(writer, PROFILE_TITLE) != 0)
        goto cleanup;

    for (size_t index = 0; index < attribute_count; ++index) {
        if (db5_update_attribute(DB5_GLOBAL_OBJECT_NAME, attributes[index].name,
                                 attributes[index].value, writer->dbip) != 0)
            goto cleanup;
    }

    result = 1;

cleanup:
    wdb_close(writer);
    return result;
}


static int
write_invalid_profile(const std::string& path)
{
    struct rt_wdb *writer = wdb_fopen(path.c_str());

    if (writer == RT_WDB_NULL)
        return 0;

    wdb_close(writer);
    return 1;
}


static void __cdecl
capture_status(const char *message)
{
    last_status = message ? message : "";
}


static void __cdecl
capture_popup(const char *UNUSED(title), const char *UNUSED(message))
{
}


static std::string
copy_option(const char *value)
{
    return value ? value : "";
}


static void __cdecl
capture_options(const struct creo_brl_conversion_options *options)
{
    if (!options)
        return;

    observed = {};
    observed.received = 1;
    observed.log_type = options->log_type;
    observed.xform_mode = options->xform_mode;
    observed.chord_mode = options->chord_mode;
    observed.flags = options->flags;
    observed.material_path = copy_option(options->material_path);
    observed.rename_parameters = copy_option(options->rename_parameters);
    observed.preserve_parameters = copy_option(options->preserve_parameters);
    observed.region_counter = copy_option(options->region_counter);
    observed.minimum_luminance = copy_option(options->minimum_luminance);
    observed.maximum_chord = copy_option(options->maximum_chord);
    observed.minimum_angle = copy_option(options->minimum_angle);
    observed.minimum_hole = copy_option(options->minimum_hole);
    observed.minimum_chamfer = copy_option(options->minimum_chamfer);
    observed.minimum_round = copy_option(options->minimum_round);
}


static void
reset_observed(void)
{
    observed = {};
    last_status.clear();
}


static int
expect_defaults(const char *context)
{
    return expect(observed.received, context) &&
           expect(observed.log_type == CREO_BRL_CONVERSION_LOG_ALL, "unexpected default log type") &&
           expect(observed.material_path == BUNDLED_MATERIAL_FILE, "unexpected default material path") &&
           expect(observed.rename_parameters == "nomenclature", "unexpected default parameter rename") &&
           expect(observed.preserve_parameters == "nomenclature,part_number,ptc_material_name",
                  "unexpected default preserved parameters") &&
           expect(observed.xform_mode == CREO_BRL_CONVERSION_XFORM_NONE,
                  "unexpected default coordinate transform") &&
           expect(observed.region_counter == "1000", "unexpected default region counter") &&
           expect(observed.minimum_luminance == "30", "unexpected default minimum luminance") &&
           expect(observed.chord_mode == CREO_BRL_CONVERSION_CHORD_RELATIVE,
                  "unexpected default chord mode") &&
           expect(observed.maximum_chord == "0.3", "unexpected default maximum chord") &&
           expect(observed.minimum_angle == "0.5", "unexpected default minimum angle") &&
           expect(observed.flags == CREO_BRL_CONVERSION_OPTION_FACETS_ONLY,
                  "unexpected default option flags") &&
           expect(observed.minimum_hole == "0.0" && observed.minimum_chamfer == "0.0" &&
                  observed.minimum_round == "0.0", "unexpected default small-feature values");
}


static struct creo_brl_conversion_options
observed_view(void)
{
    struct creo_brl_conversion_options view = {};

    view.log_type = observed.log_type;
    view.material_path = observed.material_path.c_str();
    view.rename_parameters = observed.rename_parameters.c_str();
    view.preserve_parameters = observed.preserve_parameters.c_str();
    view.xform_mode = observed.xform_mode;
    view.region_counter = observed.region_counter.c_str();
    view.minimum_luminance = observed.minimum_luminance.c_str();
    view.chord_mode = observed.chord_mode;
    view.maximum_chord = observed.maximum_chord.c_str();
    view.minimum_angle = observed.minimum_angle.c_str();
    view.flags = observed.flags;
    view.minimum_hole = observed.minimum_hole.c_str();
    view.minimum_chamfer = observed.minimum_chamfer.c_str();
    view.minimum_round = observed.minimum_round.c_str();
    return view;
}


static int
fake_set_text(void *context, const char *control, const char *value)
{
    static_cast<struct fake_controls *>(context)->text[control] = value;
    return 1;
}


static int
fake_set_check(void *context, const char *control, int checked)
{
    static_cast<struct fake_controls *>(context)->checks[control] = checked;
    return 1;
}


static int
fake_set_radio(void *context, const char *control, const char *selection)
{
    static_cast<struct fake_controls *>(context)->radios[control] = selection;
    return 1;
}


static int
fake_get_text(void *context, const char *control, std::string *value)
{
    const std::map<std::string, std::string>& text = static_cast<struct fake_controls *>(context)->text;
    const std::map<std::string, std::string>::const_iterator found = text.find(control);

    if (found == text.end())
        return 0;
    *value = found->second;
    return 1;
}


static int
fake_get_check(void *context, const char *control, int *checked)
{
    const std::map<std::string, int>& checks = static_cast<struct fake_controls *>(context)->checks;
    const std::map<std::string, int>::const_iterator found = checks.find(control);

    if (found == checks.end())
        return 0;
    *checked = found->second;
    return 1;
}


static int
fake_get_radio(void *context, const char *control, std::string *selection)
{
    const std::map<std::string, std::string>& radios = static_cast<struct fake_controls *>(context)->radios;
    const std::map<std::string, std::string>::const_iterator found = radios.find(control);

    if (found == radios.end())
        return 0;
    *selection = found->second;
    return 1;
}


static const struct creo_brl_frontend_control_api fake_control_api = {
    NULL,
    fake_set_text,
    fake_set_check,
    fake_set_radio,
    fake_get_text,
    fake_get_check,
    fake_get_radio
};


static int
path_exists(const char *path, void *context)
{
    const std::set<std::string>& paths = *static_cast<std::set<std::string> *>(context);

    return paths.find(path) != paths.end();
}


static int
test_path_precedence(void)
{
    std::set<std::string> paths;
    char selected_path[PROFILE_PATH_BUFFER_SIZE] = {'\0'};

    paths.insert("C:\\profiles\\environment.g");
    paths.insert("C:\\work\\Creo_to_BRL.g");
    paths.insert("C:\\bundle\\Creo_to_BRL.g");
    if (!expect(creo_brl_resolve_profile_path("C:\\profiles\\environment.g", "C:\\work",
                                              "C:\\bundle\\Creo_to_BRL.g", path_exists,
                                              &paths, selected_path, sizeof(selected_path)),
                "environment profile was not selected") ||
        !expect(std::string(selected_path) == "C:\\profiles\\environment.g",
                "environment profile path was not returned"))
        return 0;

    paths.clear();
    paths.insert("C:\\work\\relative.g");
    if (!expect(creo_brl_resolve_profile_path("relative.g", "C:\\work", NULL, path_exists,
                                              &paths, selected_path, sizeof(selected_path)),
                "relative environment profile was not selected") ||
        !expect(std::string(selected_path) == "C:\\work\\relative.g",
                "relative environment profile was not resolved against the working directory"))
        return 0;

    paths.clear();
    paths.insert("C:\\work\\drive_relative.g");
    if (!expect(creo_brl_resolve_profile_path("C:drive_relative.g", "C:\\work", NULL, path_exists,
                                              &paths, selected_path, sizeof(selected_path)),
                "drive-relative environment profile was not selected") ||
        !expect(std::string(selected_path) == "C:\\work\\drive_relative.g",
                "drive-relative environment profile was not resolved against the working directory"))
        return 0;

    paths.clear();
    paths.insert("C:\\work\\Creo_to_BRL.g");
    if (!expect(creo_brl_resolve_profile_path("missing.g", "C:\\work", "C:\\bundle\\Creo_to_BRL.g",
                                              path_exists, &paths, selected_path, sizeof(selected_path)),
                "working-directory profile was not selected") ||
        !expect(std::string(selected_path) == "C:\\work\\Creo_to_BRL.g",
                "working-directory profile path was not returned"))
        return 0;

    paths.clear();
    paths.insert("C:\\bundle\\Creo_to_BRL.g");
    if (!expect(creo_brl_resolve_profile_path(NULL, "C:\\work", "C:\\bundle\\Creo_to_BRL.g",
                                              path_exists, &paths, selected_path, sizeof(selected_path)),
                "bundled profile was not selected") ||
        !expect(std::string(selected_path) == "C:\\bundle\\Creo_to_BRL.g",
                "bundled profile path was not returned"))
        return 0;

    paths.clear();
    if (!expect(!creo_brl_resolve_profile_path(NULL, "C:\\work", "C:\\bundle\\Creo_to_BRL.g",
                                               path_exists, &paths, selected_path,
                                               sizeof(selected_path)) &&
                selected_path[0] == '\0',
                "missing profiles should select defaults"))
        return 0;

    selected_path[0] = 'x';
    selected_path[1] = '\0';
    if (!expect(!creo_brl_resolve_profile_path(NULL, "C:\\work", NULL, NULL, NULL,
                                               selected_path, sizeof(selected_path)) &&
                selected_path[0] == '\0',
                "invalid resolver input should clear the selected path"))
        return 0;

    paths.insert("C:\\profiles\\environment.g");
    char small_path[SMALL_PROFILE_PATH_BUFFER_SIZE] = {'x', '\0'};
    return expect(!creo_brl_resolve_profile_path("C:\\profiles\\environment.g", "C:\\work", NULL,
                                                  path_exists, &paths, small_path,
                                                  sizeof(small_path)) &&
                  small_path[0] == '\0',
                  "an oversized profile path should be rejected");
}


static int
test_selected_invalid_profile_does_not_fallback(const std::string& directory)
{
    static const struct profile_attribute fallback_attributes[] = {
        {"initial_region_counter", "2000"}
    };
    const std::string environment_profile =
        path_join(directory, SELECTED_INVALID_PROFILE_FILE_NAME);
    const std::string working_profile = path_join(directory, WORKING_PROFILE_FILE_NAME);
    const std::string bundled_profile = path_join(directory, BUNDLED_PROFILE_FILE_NAME);
    std::set<std::string> paths;
    char selected_path[PROFILE_PATH_BUFFER_SIZE] = {'\0'};

    if (!expect(write_invalid_profile(environment_profile),
                "unable to write selected invalid profile fixture") ||
        !expect(write_profile(working_profile, fallback_attributes,
                              sizeof(fallback_attributes) / sizeof(fallback_attributes[0])),
                "unable to write working-directory fallback fixture") ||
        !expect(write_profile(bundled_profile, fallback_attributes,
                              sizeof(fallback_attributes) / sizeof(fallback_attributes[0])),
                "unable to write bundled fallback fixture"))
        return 0;

    paths.insert(environment_profile);
    paths.insert(working_profile);
    paths.insert(bundled_profile);
    if (!expect(creo_brl_resolve_profile_path(environment_profile.c_str(), directory.c_str(),
                                              bundled_profile.c_str(), path_exists, &paths,
                                              selected_path, sizeof(selected_path)),
                "selected invalid profile was not resolved") ||
        !expect(std::string(selected_path) == environment_profile,
                "resolver fell through an existing invalid profile"))
        return 0;

    reset_observed();
    creo_brl_core_load_profile(selected_path, PROFILE_MATERIAL_DIRECTORY);
    return expect_defaults("selected invalid profile did not produce defaults") &&
           expect(last_status.find("Unable to read") != std::string::npos,
                  "selected invalid profile did not produce the expected diagnostic");
}


static int
test_profile_reader(const std::string& directory)
{
    static const struct profile_attribute profile_attributes[] = {
        {"process_log_criteria", "success"},
        {"material_file_name", LEGACY_MATERIAL_FILE},
        {"create_object_names", "part_number"},
        {"preserved_attributes", "name,material"},
        {"coordinate_transformation", "y_to_z"},
        {"initial_region_counter", "77"},
        {"minimum_luminance", "12"},
        {"chord_mode", "millimeter"},
        {"maximum_chord_height", "0.9"},
        {"minimum_angle_control", "0.75"},
        {"eliminate_small_features", "on"},
        {"minimum_hole_diameter", "0.1"},
        {"minimum_chamfer_dimension", "0.2"},
        {"minimum_blend_radius", "0.3"},
        {"facetize_everything", "off"},
        {"reject_failed_bots", "on"},
        {"box_replaces_failed_part", "on"},
        {"write_surface_normals", "on"}
    };
    const std::string profile_path = path_join(directory, PROFILE_FILE_NAME);

    if (!expect(write_profile(profile_path, profile_attributes,
                              sizeof(profile_attributes) / sizeof(profile_attributes[0])),
                "unable to write profile fixture"))
        return 0;

    reset_observed();
    creo_brl_core_load_profile(profile_path.c_str(), PROFILE_MATERIAL_DIRECTORY);
    return expect(observed.received, "profile reader did not publish options") &&
           expect(observed.log_type == CREO_BRL_CONVERSION_LOG_SUCCESS, "log type was not parsed") &&
           expect(observed.material_path == BUNDLED_MATERIAL_FILE, "legacy material path was not rewritten") &&
           expect(observed.rename_parameters == "part_number", "parameter rename was not parsed") &&
           expect(observed.preserve_parameters == "name,material", "preserved attributes were not parsed") &&
           expect(observed.xform_mode == CREO_BRL_CONVERSION_XFORM_Y_TO_Z, "transform was not parsed") &&
           expect(observed.region_counter == "77" && observed.minimum_luminance == "12",
                  "integer profile fields were not parsed") &&
           expect(observed.chord_mode == CREO_BRL_CONVERSION_CHORD_ABSOLUTE,
                  "chord mode was not parsed") &&
           expect(observed.maximum_chord == "0.9" && observed.minimum_angle == "0.75",
                  "tessellation fields were not parsed") &&
           expect(observed.minimum_hole == "0.1" && observed.minimum_chamfer == "0.2" &&
                  observed.minimum_round == "0.3", "small-feature fields were not parsed") &&
           expect(observed.flags == (CREO_BRL_CONVERSION_OPTION_ELIMINATE_SMALL_FEATURES |
                                     CREO_BRL_CONVERSION_OPTION_CHECK_SOLIDITY |
                                     CREO_BRL_CONVERSION_OPTION_CREATE_BOXES |
                                     CREO_BRL_CONVERSION_OPTION_WRITE_NORMALS),
                  "checkbox profile fields were not parsed");
}


static int
test_defaults_and_invalid_profiles(const std::string& directory)
{
    const std::string invalid_profile = path_join(directory, INVALID_PROFILE_FILE_NAME);

    reset_observed();
    creo_brl_core_load_profile(NULL, PROFILE_MATERIAL_DIRECTORY);
    if (!expect_defaults("defaults were not published without a profile"))
        return 0;

    if (!expect(write_invalid_profile(invalid_profile), "unable to write invalid profile fixture"))
        return 0;
    reset_observed();
    creo_brl_core_load_profile(invalid_profile.c_str(), PROFILE_MATERIAL_DIRECTORY);
    return expect_defaults("invalid profile did not produce defaults") &&
           expect(last_status.find("Unable to read") != std::string::npos,
                  "invalid profile did not produce the expected diagnostic");
}


static int
test_invalid_values(const std::string& directory)
{
    static const struct profile_attribute invalid_attributes[] = {
        {"process_log_criteria", "invalid-log-type"},
        {"coordinate_transformation", "invalid-transform"},
        {"chord_mode", "invalid-chord-mode"},
        {"maximum_chord_height", "not-a-number"},
        {"facetize_everything", "invalid-checkbox-value"}
    };
    const std::string profile_path = path_join(directory, INVALID_VALUE_PROFILE_FILE_NAME);
    struct fake_controls controls = {};
    struct creo_brl_frontend_control_api controls_api = fake_control_api;
    struct creo_brl_snapshot_settings settings = {};
    struct snapshot_settings_strings strings = {};
    struct creo_brl_conversion_options view = {};

    if (!expect(write_profile(profile_path, invalid_attributes,
                              sizeof(invalid_attributes) / sizeof(invalid_attributes[0])),
                "unable to write invalid-value profile fixture"))
        return 0;

    reset_observed();
    creo_brl_core_load_profile(profile_path.c_str(), PROFILE_MATERIAL_DIRECTORY);
    if (!expect(observed.received, "invalid-value profile did not publish options") ||
        !expect(observed.log_type == CREO_BRL_CONVERSION_LOG_ALL &&
                observed.xform_mode == CREO_BRL_CONVERSION_XFORM_NONE &&
                observed.chord_mode == CREO_BRL_CONVERSION_CHORD_RELATIVE,
                "invalid radio values did not preserve defaults") ||
        !expect((observed.flags & CREO_BRL_CONVERSION_OPTION_FACETS_ONLY) == 0,
                "invalid checkbox value did not preserve legacy unchecked behavior") ||
        !expect(observed.maximum_chord == "not-a-number",
                "invalid numeric value did not reach dialog validation"))
        return 0;

    view = observed_view();
    controls_api.context = &controls;
    if (!expect(creo_brl_frontend_apply_options_to_controls(&view, &controls_api),
                "invalid-value options did not apply to controls"))
        return 0;
    controls.text["out_fname"] = "invalid_value.g";
    controls.text["log_fname"] = "invalid_value.log";

    return expect(!creo_brl_frontend_capture_snapshot_options(&controls_api, &settings, &strings),
                  "invalid numeric profile value bypassed dialog validation");
}


static int
test_ui_mapping(const std::string& directory)
{
    struct fake_controls controls = {};
    struct creo_brl_frontend_control_api controls_api = fake_control_api;
    struct creo_brl_conversion_options view = observed_view();
    struct creo_brl_snapshot_settings settings = {};
    struct snapshot_settings_strings strings = {};
    const std::string profile_path = path_join(directory, PROFILE_FILE_NAME);
    const std::string source_before = read_file(profile_path);

    controls_api.context = &controls;
    if (!expect(creo_brl_frontend_apply_options_to_controls(&view, &controls_api),
                "semantic options did not apply to controls") ||
        !expect(controls.radios["log_file_type"] == "success" &&
                controls.radios["transform"] == "y_to_z" &&
                controls.radios["chord_mode"] == "millimeter",
                "radio options were not mapped to controls") ||
        !expect(controls.text["mtl_fname"] == BUNDLED_MATERIAL_FILE &&
                controls.text["region_counter"] == "77" && controls.checks["facets_only"] == 0,
                "profile options were not mapped to controls"))
        return 0;

    controls.text["out_fname"] = "ui_edited.g";
    controls.text["log_fname"] = "ui_edited.log";
    controls.text["region_counter"] = "501";
    controls.radios["log_file_type"] = "all/(debug)";
    controls.radios["transform"] = "x_to_z";
    controls.radios["chord_mode"] = "percent";
    controls.checks["facets_only"] = 1;

    if (!expect(creo_brl_frontend_capture_snapshot_options(&controls_api, &settings, &strings),
                "edited controls did not produce snapshot settings"))
        return 0;

    return expect(strings.output_path == "ui_edited.g" && strings.log_path == "ui_edited.log",
                  "UI edits were not embedded in snapshot strings") &&
           expect(settings.region_id == 501 &&
                  settings.log_type == CREO_BRL_SNAPSHOT_LOG_ALL &&
                  settings.xform_mode == CREO_BRL_SNAPSHOT_XFORM_X_TO_Z &&
                  settings.chord_mode == CREO_BRL_SNAPSHOT_CHORD_RELATIVE,
                  "UI edits were not embedded in snapshot settings") &&
           expect((settings.flags & CREO_BRL_SNAPSHOT_SETTING_FACETS_ONLY) != 0,
                  "UI checkbox edit was not embedded in snapshot settings") &&
           expect(source_before == read_file(profile_path),
                  "UI mapping rewrote the selected profile file");
}


static int
test_default_output_paths()
{
    std::string output_path;
    std::string log_path;

    return expect(creo_brl_frontend_default_output_paths(
                      DEFAULT_PATH_MODEL_NAME, &output_path, &log_path),
                  "model-derived output defaults were not generated") &&
           expect(output_path == DEFAULT_OUTPUT_PATH &&
                      log_path == DEFAULT_LOG_PATH,
                  "model-derived output defaults changed") &&
           expect(!creo_brl_frontend_default_output_paths(
                      NULL, &output_path, &log_path),
                  "missing model name unexpectedly produced output defaults");
}


static int
test_bundled_profile(const char *profile_path)
{
    struct db_i *database = db_open(profile_path, DB_OPEN_READONLY);
    struct directory *global = RT_DIR_NULL;
    struct bu_attribute_value_set attributes = BU_AVS_INIT_ZERO;
    int attribute_absent = 0;

    if (database != DBI_NULL && db_dirbuild(database) >= 0) {
        global = db_lookup(database, DB5_GLOBAL_OBJECT_NAME, LOOKUP_QUIET);
        attribute_absent = global != RT_DIR_NULL &&
                           db5_get_attributes(database, &attributes, global) == 0 &&
                           bu_avs_get(&attributes, OBSOLETE_STL_ATTRIBUTE) == NULL;
    }
    bu_avs_free(&attributes);
    if (database != DBI_NULL)
        db_close(database);
    if (!expect(attribute_absent,
                "bundled profile retained the obsolete STL attribute"))
        return 0;

    reset_observed();
    creo_brl_core_load_profile(profile_path, PROFILE_MATERIAL_DIRECTORY);
    return expect(observed.received, "bundled profile did not publish options") &&
           expect(observed.flags == CREO_BRL_CONVERSION_OPTION_FACETS_ONLY,
                  "bundled profile option flags changed") &&
           expect(observed.region_counter == "1000" &&
                      observed.minimum_luminance == "30" &&
                      observed.maximum_chord == "0.3" &&
                      observed.minimum_angle == "0.5",
                  "bundled profile numeric settings changed") &&
           expect(observed.minimum_hole == "0" &&
                      observed.minimum_chamfer == "0" &&
                      observed.minimum_round == "0",
                  "bundled profile small-feature settings changed");
}


int
main(int argc, char *argv[])
{
    static const struct creo_brl_frontend_api test_frontend_api = {
        capture_status,
        capture_popup,
        capture_options
    };
    const std::string directory = argc == EXPECTED_ARGUMENT_COUNT ? argv[1] : "";
    const char *bundled_profile = argc == EXPECTED_ARGUMENT_COUNT ? argv[2] : NULL;

    if (argc != EXPECTED_ARGUMENT_COUNT) {
        std::fprintf(stderr, "Usage: %s <output-directory> <bundled-profile>\n", argv[0]);
        return 1;
    }

    creo_brl_core_set_frontend_api(&test_frontend_api);
    if (!test_path_precedence())
        return 1;
    if (!test_selected_invalid_profile_does_not_fallback(directory))
        return 1;
    if (!test_profile_reader(directory))
        return 1;
    if (!test_ui_mapping(directory))
        return 1;
    if (!test_default_output_paths())
        return 1;
    if (!test_bundled_profile(bundled_profile))
        return 1;
    if (!test_defaults_and_invalid_profiles(directory))
        return 1;
    if (!test_invalid_values(directory))
        return 1;

    creo_brl_core_set_frontend_api(NULL);
    return 0;
}
