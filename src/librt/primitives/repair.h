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

#include "bu/cmdschema.h"

struct rt_repair_common_args {
    int dummy;
};

/* Primitive repair callbacks receive the complete GED argument vector so
 * their type-specific options can coexist with the generic repair options.
 * This common query intentionally only consumes its own flags. */
static int
rt_repair_common_options(struct bu_vls *log_str, int argc, const char **argv)
{
    static const struct bu_cmd_option options[] = {
	BU_CMD_FLAG("h", "help", struct rt_repair_common_args, dummy, "Print help"),
	BU_CMD_FLAG("", "options-json", struct rt_repair_common_args, dummy,
	    "Return JSON of supported options"),
	BU_CMD_OPTION_NULL
    };
    static const struct bu_cmd_schema schema = {
	"repair", "Primitive repair options", options, NULL,
	BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
    };

    if (bu_cmd_schema_option_present(&schema, (size_t)argc, argv,
	    "options-json")) {
	if (log_str)
	    bu_vls_printf(log_str, "{\"options\":[]}");
	return 1;
    }
    if (bu_cmd_schema_option_present(&schema, (size_t)argc, argv, "help")) {
	if (log_str) {
	    char *option_help = bu_cmd_schema_describe(&schema);
	    bu_vls_printf(log_str, "{\"status\":\"help\",\"message\":\"Options:\\n%s\"}",
		option_help ? option_help : "");
	    if (option_help)
		bu_free(option_help, "repair option help");
	}
	return -1;
    }

    return 0;
}

#endif /* LIBRT_PRIMITIVES_REPAIR_H */
