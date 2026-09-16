/*                S T E P _ P L U G I N _ L O A D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * Exercise a STEP schema module through the namespaced libbu registry.
 */

#include "step_plugin.h"

#include <string>

#include "bu/app.h"

static std::string allowed_path;

static int
allow_test_plugin(const char *path)
{
    return path && allowed_path == path;
}

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    if (argc == 3 && std::string(argv[2]) == "reject") {
        allowed_path = argv[1];
        bu_plugin_set_path_allow(allow_test_plugin);
        if (bu_plugin_init() != 0) return 7;
        if (bu_plugin_load(argv[1]) != -1) return 8;
        if (bu_plugin_loaded_modules_count() != 0) return 9;
        bu_plugin_shutdown();
        return 0;
    }
    if (argc != 4) return 1;
    allowed_path = argv[1];
    bu_plugin_set_path_allow(allow_test_plugin);
    if (bu_plugin_init() != 0) return 2;
    if (bu_plugin_load(argv[1]) != 2) return 3;
    if (bu_plugin_loaded_modules_count() != 1) return 4;

    const char *residual[] = {"--help"};
    struct step_import_options options = {};
    options.api_version = STEP_PLUGIN_IMPORT_OPTIONS_VERSION;
    options.struct_size = sizeof(options);
    struct step_plugin_request request = {};
    request.api_version = STEP_PLUGIN_REQUEST_VERSION;
    request.struct_size = sizeof(request);
    request.operation = STEP_OPERATION_IMPORT;
    request.input_path = "unused.step";
    request.output_path = "unused.g";
    request.schema = argv[2];
    request.import_options = &options;
    request.argc = 1;
    request.argv = residual;

    char diagnostic[256] = {0};
    struct step_plugin_result result = {
        STEP_PLUGIN_RESULT_VERSION, sizeof(struct step_plugin_result), 0, 0,
        0, 0, diagnostic, sizeof(diagnostic), 0
    };
    if (step_plugin_host_call(argv[3], &request, &result) != 0) return 5;
    if (bu_plugin_loaded_modules_count() != 1) return 6;
    bu_plugin_shutdown();
    return 0;
}
