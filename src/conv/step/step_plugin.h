/*                         S T E P _ P L U G I N . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1.
 */

#ifndef CONV_STEP_STEP_PLUGIN_H
#define CONV_STEP_STEP_PLUGIN_H

#include "common.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define STEP_PLUGIN_REQUEST_VERSION 1u
#define STEP_PLUGIN_IMPORT_OPTIONS_VERSION 2u
#define STEP_PLUGIN_EXPORT_OPTIONS_VERSION 4u
#define STEP_PLUGIN_RESULT_VERSION 1u

/* These values are part of the C ABI.  Add new values; do not renumber. */
enum step_operation {
    STEP_OPERATION_INVALID = 0,
    STEP_OPERATION_IMPORT = 1,
    STEP_OPERATION_EXPORT = 2
};

enum step_plugin_status {
    STEP_PLUGIN_STATUS_OK = 0,
    STEP_PLUGIN_STATUS_PARTIAL = 1,
    STEP_PLUGIN_STATUS_USAGE = 2,
    STEP_PLUGIN_STATUS_INPUT = 3,
    STEP_PLUGIN_STATUS_OUTPUT = 4,
    STEP_PLUGIN_STATUS_CANCELLED = 5,
    STEP_PLUGIN_STATUS_EMPTY = 6,
    STEP_PLUGIN_STATUS_ABI = 125,
    STEP_PLUGIN_STATUS_EXCEPTION = 126,
    STEP_PLUGIN_STATUS_INTERNAL = 127
};

/* Schema-neutral importer settings.  A zero value means the converter's
 * documented permissive default unless the associated setting says
 * otherwise: non-strict, non-exact, and preserve storage-safe invalid B-reps.
 * Preserved invalid geometry is an unresolved partial result, never an OK
 * result or a claim that the STEP source is invalid.  A null repair_mode
 * selects safe repair. */
struct step_import_options {
    uint32_t api_version;
    size_t struct_size;
    uint32_t dry_run;
    uint32_t verbose;
    uint32_t exact;
    uint32_t strict;
    uint32_t reject_invalid_objects;
    uint32_t overwrite;
    uint32_t requested_jobs;
    uint32_t disable_item_budgets;
    double absolute_tolerance_mm;
    double budget_scale;
    double item_budget_seconds;
    double stall_timeout_seconds;
    const char *repair_mode;
    const char *report_path;
    const char *summary_path;
    size_t selected_entity_count;
    const int64_t *selected_entity_ids;
    /* Exclude standalone OPEN_SHELL surface-model boundaries without
     * affecting CLOSED_SHELL solid boundaries. */
    uint32_t skip_open_shells;
};

/* Exporter settings stay deliberately small until StepExportPlan owns the
 * complete schema-neutral export configuration. */
struct step_export_options {
    uint32_t api_version;
    size_t struct_size;
    uint32_t overwrite;
    uint32_t verbose;
    uint32_t dry_run;
    /* Opt in to a schema-native CSG representation.  The default remains
     * the established BRep-oriented export path. */
    uint32_t native_csg;
    /** Refuse to publish an output when any selected/reachable object or
     * boolean/occurrence semantic cannot be represented. */
    uint32_t strict;
    /** One output length unit in millimetres.  A zero value requests the
     * documented millimetre default.  length_unit is the user-facing unit
     * label used to author the STEP unit name. */
    double length_unit_mm;
    /** STEP global uncertainty expressed in the selected output length unit.
     * A zero value requests the documented 0.05 mm physical default. */
    double uncertainty;
    const char *length_unit;
    /** "degree" or "radian"; null requests degrees. */
    const char *plane_angle_unit;
    const char *report_path;
    size_t object_count;
    const char * const *object_names;
};

struct step_plugin_request {
    uint32_t api_version;
    size_t struct_size;
    enum step_operation operation;
    const char *input_path;
    const char *output_path;
    const char *schema;
    const struct step_import_options *import_options;
    const struct step_export_options *export_options;
    int argc;
    const char * const *argv;
};

/* diagnostic_buffer is caller-owned.  Plugins may write to it but may neither
 * retain nor free it.  No plugin-owned allocation crosses this ABI. */
struct step_plugin_result {
    uint32_t api_version;
    size_t struct_size;
    int status;
    uint32_t reserved;
    uint64_t warning_count;
    uint64_t error_count;
    char *diagnostic_buffer;
    size_t diagnostic_capacity;
    size_t diagnostic_length;
};

#ifdef __cplusplus
extern "C" {
#endif

int step_plugin_host_call(const char *command,
                          const struct step_plugin_request *request,
                          struct step_plugin_result *result);

#ifdef __cplusplus
}
#endif

/* Inline validation gives hosts and plugins identical checks without sharing
 * a C++ runtime object or adding a second library ABI. */
static inline void
step_plugin_result_message(struct step_plugin_result *result, int status, const char *message)
{
    size_t length = message ? strlen(message) : 0;
    if (!result) return;
    result->status = status;
    result->warning_count =
        (status == STEP_PLUGIN_STATUS_PARTIAL ||
         status == STEP_PLUGIN_STATUS_EMPTY) ? 1u : 0u;
    result->error_count =
        (status && status != STEP_PLUGIN_STATUS_PARTIAL &&
         status != STEP_PLUGIN_STATUS_EMPTY) ? 1u : 0u;
    result->diagnostic_length = length;
    if (result->diagnostic_buffer && result->diagnostic_capacity) {
        size_t copy_length = length;
        if (copy_length >= result->diagnostic_capacity)
            copy_length = result->diagnostic_capacity - 1;
        if (copy_length && message)
            memcpy(result->diagnostic_buffer, message, copy_length);
        result->diagnostic_buffer[copy_length] = '\0';
    }
}

static inline int
step_plugin_validate_result(struct step_plugin_result *result)
{
    if (!result) return 0;
    if (result->api_version != STEP_PLUGIN_RESULT_VERSION ||
        result->struct_size < sizeof(struct step_plugin_result))
        return 0;
    if (result->diagnostic_capacity && !result->diagnostic_buffer)
        return 0;
    return 1;
}

static inline int
step_plugin_validate_request(const struct step_plugin_request *request,
                             struct step_plugin_result *result)
{
    if (!request) {
        step_plugin_result_message(result, STEP_PLUGIN_STATUS_ABI, "missing STEP plugin request");
        return 0;
    }
    if (request->api_version != STEP_PLUGIN_REQUEST_VERSION ||
        request->struct_size < sizeof(struct step_plugin_request)) {
        step_plugin_result_message(result, STEP_PLUGIN_STATUS_ABI, "incompatible STEP plugin request ABI");
        return 0;
    }
    if (request->operation != STEP_OPERATION_IMPORT &&
        request->operation != STEP_OPERATION_EXPORT) {
        step_plugin_result_message(result, STEP_PLUGIN_STATUS_USAGE, "invalid STEP operation");
        return 0;
    }
    if (!request->input_path || !request->output_path || !request->schema) {
        step_plugin_result_message(result, STEP_PLUGIN_STATUS_USAGE, "STEP input, output, and schema are required");
        return 0;
    }
    if (request->argc < 0 || (request->argc && !request->argv)) {
        step_plugin_result_message(result, STEP_PLUGIN_STATUS_ABI, "invalid residual STEP arguments");
        return 0;
    }
    if (request->import_options &&
        (request->import_options->api_version != STEP_PLUGIN_IMPORT_OPTIONS_VERSION ||
         request->import_options->struct_size < sizeof(struct step_import_options))) {
        step_plugin_result_message(result, STEP_PLUGIN_STATUS_ABI, "incompatible STEP import options ABI");
        return 0;
    }
    if (request->import_options && request->import_options->selected_entity_count &&
        !request->import_options->selected_entity_ids) {
        step_plugin_result_message(result, STEP_PLUGIN_STATUS_ABI, "missing selected STEP entity array");
        return 0;
    }
    if (request->export_options &&
        (request->export_options->api_version != STEP_PLUGIN_EXPORT_OPTIONS_VERSION ||
         request->export_options->struct_size < sizeof(struct step_export_options))) {
        step_plugin_result_message(result, STEP_PLUGIN_STATUS_ABI, "incompatible STEP export options ABI");
        return 0;
    }
    if (request->export_options && request->export_options->object_count &&
        !request->export_options->object_names) {
        step_plugin_result_message(result, STEP_PLUGIN_STATUS_ABI, "missing STEP export object array");
        return 0;
    }
    return 1;
}

#define BU_PLUGIN_NAME step
#define BU_PLUGIN_CMD_RET int
#define BU_PLUGIN_CMD_ARGS \
    const struct step_plugin_request *, struct step_plugin_result *
#include "../../libbu/bu_plugin.h"

#endif /* CONV_STEP_STEP_PLUGIN_H */
