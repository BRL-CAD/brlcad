/*                        R E P A I R . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef LIBRT_PRIMITIVES_REPAIR_H
#define LIBRT_PRIMITIVES_REPAIR_H

#include "bu/arg.h"
#include "bu/malloc.h"

#include "edit_private.h"

struct rt_repair_common_args {
    int print_help;
    int options_json;
};

static int
rt_repair_common_options(struct bu_vls *log_str, int argc, const char **argv)
{
    static const struct bu_arg_option options[] = {
	BU_ARG_FLAG("h", "help", struct rt_repair_common_args, print_help, "Print help"),
	BU_ARG_FLAG("", "options-json", struct rt_repair_common_args, options_json,
	    "Return JSON of supported options"),
	BU_ARG_OPTION_NULL
    };
    static const struct bu_arg_schema schema = {
	"repair", "Primitive repair options", options, NULL,
	BU_ARG_PARSE_INTERSPERSED, BU_ARG_SCHEMA_CONSTRAINTS(NULL, NULL)
    };

    struct rt_repair_common_args args = {0, 0};

    if (edit_repair_parse_options(log_str, argc, argv, &schema, &args) != BRLCAD_OK)
	return -1;

    if (args.options_json) {
	if (log_str)
	    bu_vls_printf(log_str, "{\"options\":[]}");
	return 1;
    }
    if (args.print_help) {
	if (log_str) {
	    char *help = bu_arg_schema_help(&schema, NULL);
	    bu_vls_strcat(log_str, "{\"status\":\"help\",\"message\":");
	    bu_arg_json_string(log_str, help);
	    bu_vls_putc(log_str, '}');
	    if (help)
		bu_free(help, "repair help");
	}
	return -1;
    }

    return 0;
}

#endif /* LIBRT_PRIMITIVES_REPAIR_H */
