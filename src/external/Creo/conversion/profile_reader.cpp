/**
 * Toolkit-free BRL-CAD profile parsing for the runtime-loaded backend.
 */

#include "common.h"

#include <stdarg.h>
#include <stdio.h>
#include <string>

extern "C" {
#include "bu.h"
#include "raytrace.h"
#include "wdb.h"
}

#include "shared/frontend_api.h"
#include "shared/conversion_options.h"
#include "shared/core_api.h"


static const size_t PROFILE_MESSAGE_MAX = 4096;
static const char PROFILE_LEGACY_MATERIAL_FILE[] = "./ptc_materials.mtl";
static const char PROFILE_BUNDLED_MATERIAL_FILE[] = "ptc_materials.mtl";
static const char PROFILE_DEFAULT_MATERIAL_FILE[] = "./ptc_materials.mtl";
static const char PROFILE_DEFAULT_RENAME_PARAMETERS[] = "nomenclature";
static const char PROFILE_DEFAULT_PRESERVE_PARAMETERS[] =
    "nomenclature,part_number,ptc_material_name";
static const char PROFILE_DEFAULT_REGION_COUNTER[] = "1000";
static const char PROFILE_DEFAULT_MINIMUM_LUMINANCE[] = "30";
static const char PROFILE_DEFAULT_MAXIMUM_CHORD[] = "0.3";
static const char PROFILE_DEFAULT_MINIMUM_ANGLE[] = "0.5";
static const char PROFILE_DEFAULT_MINIMUM_FEATURE_SIZE[] = "0.0";

static const char ATTRIBUTE_LOG_TYPE[] = "process_log_criteria";
static const char ATTRIBUTE_MATERIAL_PATH[] = "material_file_name";
static const char ATTRIBUTE_RENAME_PARAMETERS[] = "create_object_names";
static const char ATTRIBUTE_PRESERVE_PARAMETERS[] = "preserved_attributes";
static const char ATTRIBUTE_TRANSFORM[] = "coordinate_transformation";
static const char ATTRIBUTE_REGION_COUNTER[] = "initial_region_counter";
static const char ATTRIBUTE_MINIMUM_LUMINANCE[] = "minimum_luminance";
static const char ATTRIBUTE_CHORD_MODE[] = "chord_mode";
static const char ATTRIBUTE_MAXIMUM_CHORD[] = "maximum_chord_height";
static const char ATTRIBUTE_MINIMUM_ANGLE[] = "minimum_angle_control";
static const char ATTRIBUTE_ELIMINATE_SMALL_FEATURES[] = "eliminate_small_features";
static const char ATTRIBUTE_MINIMUM_HOLE[] = "minimum_hole_diameter";
static const char ATTRIBUTE_MINIMUM_CHAMFER[] = "minimum_chamfer_dimension";
static const char ATTRIBUTE_MINIMUM_ROUND[] = "minimum_blend_radius";
static const char ATTRIBUTE_FACETS_ONLY[] = "facetize_everything";
static const char ATTRIBUTE_CHECK_SOLIDITY[] = "reject_failed_bots";
static const char ATTRIBUTE_CREATE_BOXES[] = "box_replaces_failed_part";
static const char ATTRIBUTE_WRITE_NORMALS[] = "write_surface_normals";

static const char *const PROFILE_ATTRIBUTE_NAMES[] = {
    ATTRIBUTE_LOG_TYPE,
    ATTRIBUTE_MATERIAL_PATH,
    ATTRIBUTE_RENAME_PARAMETERS,
    ATTRIBUTE_PRESERVE_PARAMETERS,
    ATTRIBUTE_TRANSFORM,
    ATTRIBUTE_REGION_COUNTER,
    ATTRIBUTE_MINIMUM_LUMINANCE,
    ATTRIBUTE_CHORD_MODE,
    ATTRIBUTE_MAXIMUM_CHORD,
    ATTRIBUTE_MINIMUM_ANGLE,
    ATTRIBUTE_ELIMINATE_SMALL_FEATURES,
    ATTRIBUTE_MINIMUM_HOLE,
    ATTRIBUTE_MINIMUM_CHAMFER,
    ATTRIBUTE_MINIMUM_ROUND,
    ATTRIBUTE_FACETS_ONLY,
    ATTRIBUTE_CHECK_SOLIDITY,
    ATTRIBUTE_CREATE_BOXES,
    ATTRIBUTE_WRITE_NORMALS,
    NULL
};

static const char PROFILE_VALUE_ON[] = "on";
static const char PROFILE_VALUE_LOG_FAILURE[] = "failure";
static const char PROFILE_VALUE_LOG_SUCCESS[] = "success";
static const char PROFILE_VALUE_LOG_FAILURE_OR_SUCCESS[] = "failure/success";
static const char PROFILE_VALUE_LOG_ALL[] = "all/(debug)";
static const char PROFILE_VALUE_XFORM_NONE[] = "none";
static const char PROFILE_VALUE_XFORM_X_TO_Z[] = "x_to_z";
static const char PROFILE_VALUE_XFORM_Y_TO_Z[] = "y_to_z";
static const char PROFILE_VALUE_CHORD_ABSOLUTE[] = "millimeter";
static const char PROFILE_VALUE_CHORD_RELATIVE[] = "percent";

static struct creo_brl_frontend_api frontend_api = {};


struct profile_options {
    struct creo_brl_conversion_options view;
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

    void synchronize();
    void reset(const char *material_directory);
};


static void profile_status(const char *format, ...);
static int load_profile_file(const char *profile_path,
                             const char *material_directory,
                             struct profile_options *options);


static std::string
bundled_material_path(const char *material_directory)
{
    std::string material_path = material_directory ? material_directory : "";

    if (material_path.empty())
        return PROFILE_DEFAULT_MATERIAL_FILE;

    if (material_path[material_path.size() - 1] != '/' &&
        material_path[material_path.size() - 1] != '\\')
        material_path += '\\';
    material_path += PROFILE_BUNDLED_MATERIAL_FILE;
    return material_path;
}


void
profile_options::synchronize()
{
    view.material_path = material_path.c_str();
    view.rename_parameters = rename_parameters.c_str();
    view.preserve_parameters = preserve_parameters.c_str();
    view.region_counter = region_counter.c_str();
    view.minimum_luminance = minimum_luminance.c_str();
    view.maximum_chord = maximum_chord.c_str();
    view.minimum_angle = minimum_angle.c_str();
    view.minimum_hole = minimum_hole.c_str();
    view.minimum_chamfer = minimum_chamfer.c_str();
    view.minimum_round = minimum_round.c_str();
}


void
profile_options::reset(const char *material_directory)
{
    material_path = bundled_material_path(material_directory);
    rename_parameters = PROFILE_DEFAULT_RENAME_PARAMETERS;
    preserve_parameters = PROFILE_DEFAULT_PRESERVE_PARAMETERS;
    region_counter = PROFILE_DEFAULT_REGION_COUNTER;
    minimum_luminance = PROFILE_DEFAULT_MINIMUM_LUMINANCE;
    maximum_chord = PROFILE_DEFAULT_MAXIMUM_CHORD;
    minimum_angle = PROFILE_DEFAULT_MINIMUM_ANGLE;
    minimum_hole = PROFILE_DEFAULT_MINIMUM_FEATURE_SIZE;
    minimum_chamfer = PROFILE_DEFAULT_MINIMUM_FEATURE_SIZE;
    minimum_round = PROFILE_DEFAULT_MINIMUM_FEATURE_SIZE;
    view.log_type = CREO_BRL_CONVERSION_LOG_ALL;
    view.xform_mode = CREO_BRL_CONVERSION_XFORM_NONE;
    view.chord_mode = CREO_BRL_CONVERSION_CHORD_RELATIVE;
    view.flags = CREO_BRL_CONVERSION_OPTION_FACETS_ONLY;
    synchronize();
}


static void
profile_status(const char *format, ...)
{
    char message[PROFILE_MESSAGE_MAX] = {'\0'};
    va_list arguments;

    va_start(arguments, format);
    vsprintf_s(message, sizeof(message), format, arguments);
    va_end(arguments);

    if (frontend_api.show_status)
        frontend_api.show_status(message);
}


static void
apply_checkbox(const char *value, uint32_t flag, uint32_t *flags)
{
    if (!value || !flags)
        return;

    if (bu_strcmp(value, PROFILE_VALUE_ON) == 0)
        *flags |= flag;
    else
        *flags &= ~flag;
}


static int
parse_log_type(const char *value, int32_t *log_type)
{
    if (!value || !log_type)
        return 0;

    if (bu_strcmp(value, PROFILE_VALUE_LOG_FAILURE) == 0)
        *log_type = CREO_BRL_CONVERSION_LOG_FAILURE;
    else if (bu_strcmp(value, PROFILE_VALUE_LOG_SUCCESS) == 0)
        *log_type = CREO_BRL_CONVERSION_LOG_SUCCESS;
    else if (bu_strcmp(value, PROFILE_VALUE_LOG_FAILURE_OR_SUCCESS) == 0)
        *log_type = CREO_BRL_CONVERSION_LOG_FAILURE_OR_SUCCESS;
    else if (bu_strcmp(value, PROFILE_VALUE_LOG_ALL) == 0)
        *log_type = CREO_BRL_CONVERSION_LOG_ALL;
    else
        return 0;

    return 1;
}


static int
parse_xform_mode(const char *value, int32_t *xform_mode)
{
    if (!value || !xform_mode)
        return 0;

    if (bu_strcmp(value, PROFILE_VALUE_XFORM_X_TO_Z) == 0)
        *xform_mode = CREO_BRL_CONVERSION_XFORM_X_TO_Z;
    else if (bu_strcmp(value, PROFILE_VALUE_XFORM_Y_TO_Z) == 0)
        *xform_mode = CREO_BRL_CONVERSION_XFORM_Y_TO_Z;
    else if (bu_strcmp(value, PROFILE_VALUE_XFORM_NONE) == 0)
        *xform_mode = CREO_BRL_CONVERSION_XFORM_NONE;
    else
        return 0;

    return 1;
}


static int
parse_chord_mode(const char *value, int32_t *chord_mode)
{
    if (!value || !chord_mode)
        return 0;

    if (bu_strcmp(value, PROFILE_VALUE_CHORD_ABSOLUTE) == 0)
        *chord_mode = CREO_BRL_CONVERSION_CHORD_ABSOLUTE;
    else if (bu_strcmp(value, PROFILE_VALUE_CHORD_RELATIVE) == 0)
        *chord_mode = CREO_BRL_CONVERSION_CHORD_RELATIVE;
    else
        return 0;

    return 1;
}


static int
has_profile_attributes(const struct bu_attribute_value_set *attributes)
{
    if (!attributes)
        return 0;

    for (const char *const *attribute = PROFILE_ATTRIBUTE_NAMES; *attribute; ++attribute) {
        if (bu_avs_get(attributes, *attribute))
            return 1;
    }

    return 0;
}


static void
apply_profile_attributes(const struct bu_attribute_value_set *attributes,
                         const char *material_directory,
                         struct profile_options *options)
{
    const char *value = NULL;

    if (!attributes || !options)
        return;

    value = bu_avs_get(attributes, ATTRIBUTE_LOG_TYPE);
    (void)parse_log_type(value, &options->view.log_type);
    if ((value = bu_avs_get(attributes, ATTRIBUTE_MATERIAL_PATH)))
        options->material_path = bu_strcmp(value, PROFILE_LEGACY_MATERIAL_FILE) == 0 ?
                                     bundled_material_path(material_directory) : value;
    if ((value = bu_avs_get(attributes, ATTRIBUTE_RENAME_PARAMETERS)))
        options->rename_parameters = value;
    if ((value = bu_avs_get(attributes, ATTRIBUTE_PRESERVE_PARAMETERS)))
        options->preserve_parameters = value;
    value = bu_avs_get(attributes, ATTRIBUTE_TRANSFORM);
    (void)parse_xform_mode(value, &options->view.xform_mode);
    if ((value = bu_avs_get(attributes, ATTRIBUTE_REGION_COUNTER)))
        options->region_counter = value;
    if ((value = bu_avs_get(attributes, ATTRIBUTE_MINIMUM_LUMINANCE)))
        options->minimum_luminance = value;
    value = bu_avs_get(attributes, ATTRIBUTE_CHORD_MODE);
    (void)parse_chord_mode(value, &options->view.chord_mode);
    if ((value = bu_avs_get(attributes, ATTRIBUTE_MAXIMUM_CHORD)))
        options->maximum_chord = value;
    if ((value = bu_avs_get(attributes, ATTRIBUTE_MINIMUM_ANGLE)))
        options->minimum_angle = value;
    apply_checkbox(bu_avs_get(attributes, ATTRIBUTE_ELIMINATE_SMALL_FEATURES),
                   CREO_BRL_CONVERSION_OPTION_ELIMINATE_SMALL_FEATURES, &options->view.flags);
    if ((value = bu_avs_get(attributes, ATTRIBUTE_MINIMUM_HOLE)))
        options->minimum_hole = value;
    if ((value = bu_avs_get(attributes, ATTRIBUTE_MINIMUM_CHAMFER)))
        options->minimum_chamfer = value;
    if ((value = bu_avs_get(attributes, ATTRIBUTE_MINIMUM_ROUND)))
        options->minimum_round = value;
    apply_checkbox(bu_avs_get(attributes, ATTRIBUTE_FACETS_ONLY),
                   CREO_BRL_CONVERSION_OPTION_FACETS_ONLY, &options->view.flags);
    apply_checkbox(bu_avs_get(attributes, ATTRIBUTE_CHECK_SOLIDITY),
                   CREO_BRL_CONVERSION_OPTION_CHECK_SOLIDITY, &options->view.flags);
    apply_checkbox(bu_avs_get(attributes, ATTRIBUTE_CREATE_BOXES),
                   CREO_BRL_CONVERSION_OPTION_CREATE_BOXES, &options->view.flags);
    apply_checkbox(bu_avs_get(attributes, ATTRIBUTE_WRITE_NORMALS),
                   CREO_BRL_CONVERSION_OPTION_WRITE_NORMALS, &options->view.flags);
    options->synchronize();
}


static int
load_profile_file(const char *profile_path,
                  const char *material_directory,
                  struct profile_options *options)
{
    struct bu_attribute_value_set attributes = BU_AVS_INIT_ZERO;
    struct db_i *database = NULL;
    struct directory *global_directory = NULL;
    int attributes_loaded = 0;
    int result = 0;

    database = db_open(profile_path, DB_OPEN_READONLY);
    if (database == DBI_NULL)
        goto cleanup;
    RT_CK_DBI(database);
    if (db_dirbuild(database) < 0)
        goto cleanup;
    global_directory = db_lookup(database, DB5_GLOBAL_OBJECT_NAME, LOOKUP_QUIET);
    if (global_directory == RT_DIR_NULL ||
        db5_get_attributes(database, &attributes, global_directory) != 0)
        goto cleanup;

    attributes_loaded = 1;
    if (!has_profile_attributes(&attributes))
        goto cleanup;
    apply_profile_attributes(&attributes, material_directory, options);
    result = 1;

cleanup:
    if (attributes_loaded)
        bu_avs_free(&attributes);
    if (database)
        db_close(database);
    return result;
}


extern "C" CREO_BRL_CORE_EXPORT void CREO_BRL_CORE_CALL
creo_brl_core_set_frontend_api(const struct creo_brl_frontend_api *api)
{
    if (api)
        frontend_api = *api;
    else
        frontend_api = {};
}


extern "C" CREO_BRL_CORE_EXPORT void CREO_BRL_CORE_CALL
creo_brl_core_load_profile(const char *profile_path, const char *material_directory)
{
    struct profile_options options = {};

    options.reset(material_directory);
    if (!profile_path || !profile_path[0])
        profile_status("No Creo-BRL profile was found; using defaults.");
    else if (!load_profile_file(profile_path, material_directory, &options))
        profile_status("Unable to read Creo-BRL profile '%s'; using defaults.", profile_path);
    else
        profile_status("Creo-BRL profile is '%s'.", profile_path);

    if (frontend_api.apply_options)
        frontend_api.apply_options(&options.view);
    else
        profile_status("Unable to apply Creo-BRL profile: no frontend is available.");
}
