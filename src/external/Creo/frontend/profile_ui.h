#ifndef CREO_FRONTEND_PROFILE_UI_H
#define CREO_FRONTEND_PROFILE_UI_H

#include <string>

#include "conversion_snapshot.h"
#include "shared/conversion_options.h"

struct creo_brl_frontend_control_api {
    void *context;
    int (*set_text)(void *context, const char *control, const char *value);
    int (*set_check)(void *context, const char *control, int checked);
    int (*set_radio)(void *context, const char *control, const char *selection);
    int (*get_text)(void *context, const char *control, std::string *value);
    int (*get_check)(void *context, const char *control, int *checked);
    int (*get_radio)(void *context, const char *control, std::string *selection);
};

struct creo_brl_frontend_snapshot_options {
    struct creo_brl_snapshot_settings settings;
    std::string output_path;
    std::string log_path;
    std::string material_path;
    std::string rename_parameters;
    std::string preserve_parameters;
};

int creo_brl_frontend_apply_options_to_controls(
    const struct creo_brl_conversion_options *options,
    const struct creo_brl_frontend_control_api *controls);
int creo_brl_frontend_capture_snapshot_options(
    const struct creo_brl_frontend_control_api *controls,
    struct creo_brl_frontend_snapshot_options *options);

#endif /* CREO_FRONTEND_PROFILE_UI_H */
