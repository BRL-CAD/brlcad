/*                 S T E P _ P L U G I N _ A P I . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * Test the STEP-specific libbu plugin ABI without a schema binding.
 */

#include "step_plugin.h"

#include <stdexcept>
#include <string.h>

#include "bu/app.h"

static int
successful_command(const struct step_plugin_request *, struct step_plugin_result *result)
{
    step_plugin_result_message(result, STEP_PLUGIN_STATUS_OK, "ok");
    return STEP_PLUGIN_STATUS_OK;
}

static int
throwing_command(const struct step_plugin_request *, struct step_plugin_result *)
{
    throw std::runtime_error("caught test exception");
}

int
main(int, char **argv)
{
    bu_setprogname(argv[0]);
    if (bu_plugin_init() != 0 ||
        bu_plugin_cmd_register("step.test.success", successful_command) != 0 ||
        bu_plugin_cmd_register("step.test.throw", throwing_command) != 0)
        return 1;

    char diagnostic[128] = {0};
    struct step_plugin_result result = {
        STEP_PLUGIN_RESULT_VERSION, sizeof(struct step_plugin_result), 0, 0,
        0, 0, diagnostic, sizeof(diagnostic), 0
    };
    struct step_import_options options = {};
    options.api_version = STEP_PLUGIN_IMPORT_OPTIONS_VERSION;
    options.struct_size = sizeof(options);
    struct step_plugin_request request = {};
    request.api_version = STEP_PLUGIN_REQUEST_VERSION;
    request.struct_size = sizeof(request);
    request.operation = STEP_OPERATION_IMPORT;
    request.input_path = "input.step";
    request.output_path = "output.g";
    request.schema = "ap203";
    request.import_options = &options;

    if (step_plugin_host_call("step.test.success", &request, &result) != 0)
        return 2;

    request.api_version++;
    if (step_plugin_host_call("step.test.success", &request, &result) != STEP_PLUGIN_STATUS_ABI)
        return 3;
    request.api_version = STEP_PLUGIN_REQUEST_VERSION;

    if (step_plugin_host_call("step.test.throw", &request, &result) != STEP_PLUGIN_STATUS_EXCEPTION)
        return 4;
    if (!strstr(diagnostic, "caught test exception"))
        return 5;

    result.struct_size--;
    if (step_plugin_host_call("step.test.success", &request, &result) != STEP_PLUGIN_STATUS_ABI)
        return 6;
    return 0;
}
