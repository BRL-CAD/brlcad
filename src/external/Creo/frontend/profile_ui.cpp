/**
 * Toolkit-free mapping between semantic conversion options and Creo controls.
 */

#include "profile_ui.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>


static const char CONTROL_LOG_TYPE[] = "log_file_type";
static const char CONTROL_MATERIAL_PATH[] = "mtl_fname";
static const char CONTROL_PARAMETER_RENAME[] = "param_rename";
static const char CONTROL_PARAMETER_SAVE[] = "param_save";
static const char CONTROL_TRANSFORM[] = "transform";
static const char CONTROL_REGION_COUNTER[] = "region_counter";
static const char CONTROL_MINIMUM_LUMINANCE[] = "min_luminance";
static const char CONTROL_CHORD_MODE[] = "chord_mode";
static const char CONTROL_MAXIMUM_CHORD[] = "max_chord";
static const char CONTROL_MINIMUM_ANGLE[] = "min_angle";
static const char CONTROL_ELIMINATE_SMALL_FEATURES[] = "elim_small";
static const char CONTROL_MINIMUM_HOLE[] = "min_hole";
static const char CONTROL_MINIMUM_CHAMFER[] = "min_chamfer";
static const char CONTROL_MINIMUM_ROUND[] = "min_round";
static const char CONTROL_FACETS_ONLY[] = "facets_only";
static const char CONTROL_CHECK_SOLIDITY[] = "check_solidity";
static const char CONTROL_CREATE_BOXES[] = "create_boxes";
static const char CONTROL_WRITE_NORMALS[] = "write_normals";
static const char CONTROL_OUTPUT_PATH[] = "out_fname";
static const char CONTROL_LOG_PATH[] = "log_fname";

static const char RADIO_LOG_FAILURE[] = "failure";
static const char RADIO_LOG_SUCCESS[] = "success";
static const char RADIO_LOG_FAILURE_OR_SUCCESS[] = "failure/success";
static const char RADIO_LOG_ALL[] = "all/(debug)";
static const char RADIO_TRANSFORM_NONE[] = "none";
static const char RADIO_TRANSFORM_X_TO_Z[] = "x_to_z";
static const char RADIO_TRANSFORM_Y_TO_Z[] = "y_to_z";
static const char RADIO_CHORD_ABSOLUTE[] = "millimeter";
static const char RADIO_CHORD_RELATIVE[] = "percent";
static const char DEFAULT_OUTPUT_SUFFIX[] = ".g";
static const char DEFAULT_LOG_SUFFIX[] = "_log.txt";

int
creo_brl_frontend_default_output_paths(const char *model_name,
                                       std::string *output_path,
                                       std::string *log_path)
{
    if (!model_name || !model_name[0] || !output_path || !log_path)
        return 0;

    std::string base_name(model_name);
    std::transform(base_name.begin(), base_name.end(), base_name.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });

    const std::string::size_type extension_position = base_name.find_last_of('.');
    if (extension_position != std::string::npos)
        base_name.erase(extension_position);

    *output_path = base_name + DEFAULT_OUTPUT_SUFFIX;
    *log_path = base_name + DEFAULT_LOG_SUFFIX;
    return 1;
}


static int
set_text(const struct creo_brl_frontend_control_api *controls,
         const char *control,
         const char *value)
{
    return controls && controls->set_text && value &&
           controls->set_text(controls->context, control, value);
}


static int
set_check(const struct creo_brl_frontend_control_api *controls,
          const char *control,
          uint32_t flags,
          uint32_t flag)
{
    return controls && controls->set_check &&
           controls->set_check(controls->context, control, (flags & flag) != 0);
}


static const char *
log_type_selection(int32_t log_type)
{
    switch (log_type) {
        case CREO_BRL_CONVERSION_LOG_FAILURE:
            return RADIO_LOG_FAILURE;
        case CREO_BRL_CONVERSION_LOG_SUCCESS:
            return RADIO_LOG_SUCCESS;
        case CREO_BRL_CONVERSION_LOG_FAILURE_OR_SUCCESS:
            return RADIO_LOG_FAILURE_OR_SUCCESS;
        case CREO_BRL_CONVERSION_LOG_ALL:
            return RADIO_LOG_ALL;
        default:
            return RADIO_LOG_ALL;
    }
}


static const char *
xform_selection(int32_t xform_mode)
{
    switch (xform_mode) {
        case CREO_BRL_CONVERSION_XFORM_X_TO_Z:
            return RADIO_TRANSFORM_X_TO_Z;
        case CREO_BRL_CONVERSION_XFORM_Y_TO_Z:
            return RADIO_TRANSFORM_Y_TO_Z;
        default:
            return RADIO_TRANSFORM_NONE;
    }
}


static const char *
chord_selection(int32_t chord_mode)
{
    return chord_mode == CREO_BRL_CONVERSION_CHORD_ABSOLUTE ?
               RADIO_CHORD_ABSOLUTE : RADIO_CHORD_RELATIVE;
}


static int
read_text(const struct creo_brl_frontend_control_api *controls,
          const char *control,
          std::string *value)
{
    return controls && controls->get_text && value &&
           controls->get_text(controls->context, control, value);
}


static int
read_number(const struct creo_brl_frontend_control_api *controls,
            const char *control,
            double *value)
{
    std::string text;
    char *end = NULL;

    if (!value || !read_text(controls, control, &text) || text.empty())
        return 0;

    *value = std::strtod(text.c_str(), &end);
    return end && *end == '\0' && std::isfinite(*value);
}


static int
read_integer(const struct creo_brl_frontend_control_api *controls,
             const char *control,
             int32_t *value)
{
    std::string text;
    char *end = NULL;
    long parsed = 0;

    if (!value || !read_text(controls, control, &text) || text.empty())
        return 0;

    parsed = std::strtol(text.c_str(), &end, 10);
    if (!end || *end != '\0' || parsed < std::numeric_limits<int32_t>::min() ||
        parsed > std::numeric_limits<int32_t>::max())
        return 0;

    *value = static_cast<int32_t>(parsed);
    return 1;
}


static int
read_check(const struct creo_brl_frontend_control_api *controls,
           const char *control,
           uint32_t flag,
           uint32_t *flags)
{
    int checked = 0;

    if (!controls || !controls->get_check || !flags ||
        !controls->get_check(controls->context, control, &checked))
        return 0;

    if (checked)
        *flags |= flag;

    return 1;
}


static int
read_radio(const struct creo_brl_frontend_control_api *controls,
           const char *control,
           std::string *selection)
{
    return controls && controls->get_radio && selection &&
           controls->get_radio(controls->context, control, selection) && !selection->empty();
}


static int
snapshot_log_type(const std::string& selection, int32_t *log_type)
{
    if (selection == RADIO_LOG_FAILURE)
        *log_type = CREO_BRL_SNAPSHOT_LOG_FAILURE;
    else if (selection == RADIO_LOG_SUCCESS)
        *log_type = CREO_BRL_SNAPSHOT_LOG_SUCCESS;
    else if (selection == RADIO_LOG_FAILURE_OR_SUCCESS)
        *log_type = CREO_BRL_SNAPSHOT_LOG_FAILURE_OR_SUCCESS;
    else if (selection == RADIO_LOG_ALL)
        *log_type = CREO_BRL_SNAPSHOT_LOG_ALL;
    else
        return 0;

    return 1;
}


int
creo_brl_frontend_apply_options_to_controls(
    const struct creo_brl_conversion_options *options,
    const struct creo_brl_frontend_control_api *controls)
{
    return options && controls && controls->set_radio &&
           controls->set_radio(controls->context, CONTROL_LOG_TYPE,
                               log_type_selection(options->log_type)) &&
           set_text(controls, CONTROL_MATERIAL_PATH, options->material_path) &&
           set_text(controls, CONTROL_PARAMETER_RENAME, options->rename_parameters) &&
           set_text(controls, CONTROL_PARAMETER_SAVE, options->preserve_parameters) &&
           controls->set_radio(controls->context, CONTROL_TRANSFORM,
                               xform_selection(options->xform_mode)) &&
           set_text(controls, CONTROL_REGION_COUNTER, options->region_counter) &&
           set_text(controls, CONTROL_MINIMUM_LUMINANCE, options->minimum_luminance) &&
           controls->set_radio(controls->context, CONTROL_CHORD_MODE,
                               chord_selection(options->chord_mode)) &&
           set_text(controls, CONTROL_MAXIMUM_CHORD, options->maximum_chord) &&
           set_text(controls, CONTROL_MINIMUM_ANGLE, options->minimum_angle) &&
           set_check(controls, CONTROL_ELIMINATE_SMALL_FEATURES, options->flags,
                     CREO_BRL_CONVERSION_OPTION_ELIMINATE_SMALL_FEATURES) &&
           set_text(controls, CONTROL_MINIMUM_HOLE, options->minimum_hole) &&
           set_text(controls, CONTROL_MINIMUM_CHAMFER, options->minimum_chamfer) &&
           set_text(controls, CONTROL_MINIMUM_ROUND, options->minimum_round) &&
           set_check(controls, CONTROL_FACETS_ONLY, options->flags,
                     CREO_BRL_CONVERSION_OPTION_FACETS_ONLY) &&
           set_check(controls, CONTROL_CHECK_SOLIDITY, options->flags,
                     CREO_BRL_CONVERSION_OPTION_CHECK_SOLIDITY) &&
           set_check(controls, CONTROL_CREATE_BOXES, options->flags,
                     CREO_BRL_CONVERSION_OPTION_CREATE_BOXES) &&
           set_check(controls, CONTROL_WRITE_NORMALS, options->flags,
                     CREO_BRL_CONVERSION_OPTION_WRITE_NORMALS);
}


int
creo_brl_frontend_capture_snapshot_options(
    const struct creo_brl_frontend_control_api *controls,
    struct creo_brl_frontend_snapshot_options *options)
{
    std::string selection;
    struct creo_brl_snapshot_settings *settings = NULL;

    if (!controls || !options)
        return 0;

    *options = {};
    settings = &options->settings;

    if (!read_text(controls, CONTROL_OUTPUT_PATH, &options->output_path) || options->output_path.empty() ||
        !read_text(controls, CONTROL_LOG_PATH, &options->log_path) ||
        !read_text(controls, CONTROL_MATERIAL_PATH, &options->material_path) ||
        !read_text(controls, CONTROL_PARAMETER_RENAME, &options->rename_parameters) ||
        !read_text(controls, CONTROL_PARAMETER_SAVE, &options->preserve_parameters) ||
        !read_integer(controls, CONTROL_REGION_COUNTER, &settings->region_id) ||
        !read_integer(controls, CONTROL_MINIMUM_LUMINANCE, &settings->min_luminance) ||
        !read_number(controls, CONTROL_MAXIMUM_CHORD, &settings->max_chord) ||
        !read_number(controls, CONTROL_MINIMUM_ANGLE, &settings->min_angle) ||
        !read_radio(controls, CONTROL_LOG_TYPE, &selection) ||
        !snapshot_log_type(selection, &settings->log_type) ||
        !read_radio(controls, CONTROL_TRANSFORM, &selection))
        return 0;

    settings->xform_mode = selection == RADIO_TRANSFORM_X_TO_Z ? CREO_BRL_SNAPSHOT_XFORM_X_TO_Z :
                           selection == RADIO_TRANSFORM_Y_TO_Z ? CREO_BRL_SNAPSHOT_XFORM_Y_TO_Z :
                                                                   CREO_BRL_SNAPSHOT_XFORM_NONE;
    if (!read_radio(controls, CONTROL_CHORD_MODE, &selection))
        return 0;
    settings->chord_mode = selection == RADIO_CHORD_RELATIVE ? CREO_BRL_SNAPSHOT_CHORD_RELATIVE :
                                                               CREO_BRL_SNAPSHOT_CHORD_ABSOLUTE;
    if (settings->min_luminance < 0 ||
        settings->min_luminance > CREO_BRL_SNAPSHOT_LUMINANCE_MAXIMUM ||
        settings->max_chord <= 0.0 ||
        (settings->chord_mode == CREO_BRL_SNAPSHOT_CHORD_RELATIVE &&
         settings->max_chord > CREO_BRL_SNAPSHOT_PERCENT_MAXIMUM) ||
        settings->min_angle <= 0.0 ||
        settings->min_angle > CREO_BRL_SNAPSHOT_ANGLE_MAXIMUM)
        return 0;

    if (!read_check(controls, CONTROL_ELIMINATE_SMALL_FEATURES,
                    CREO_BRL_SNAPSHOT_SETTING_ELIMINATE_SMALL_FEATURES, &settings->flags))
        return 0;
    if ((settings->flags & CREO_BRL_SNAPSHOT_SETTING_ELIMINATE_SMALL_FEATURES) != 0 &&
        (!read_number(controls, CONTROL_MINIMUM_HOLE, &settings->min_hole) ||
         !read_number(controls, CONTROL_MINIMUM_CHAMFER, &settings->min_chamfer) ||
         !read_number(controls, CONTROL_MINIMUM_ROUND, &settings->min_round) ||
         settings->min_hole < 0.0 || settings->min_chamfer < 0.0 ||
         settings->min_round < 0.0))
        return 0;

    return read_check(controls, CONTROL_FACETS_ONLY,
                      CREO_BRL_SNAPSHOT_SETTING_FACETS_ONLY, &settings->flags) &&
           read_check(controls, CONTROL_CHECK_SOLIDITY,
                      CREO_BRL_SNAPSHOT_SETTING_CHECK_SOLIDITY, &settings->flags) &&
           read_check(controls, CONTROL_CREATE_BOXES,
                      CREO_BRL_SNAPSHOT_SETTING_CREATE_BOXES, &settings->flags) &&
           read_check(controls, CONTROL_WRITE_NORMALS,
                      CREO_BRL_SNAPSHOT_SETTING_WRITE_NORMALS, &settings->flags);
}
