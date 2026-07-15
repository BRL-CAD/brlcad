/*                         P L O T 3 . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include <stdio.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/log.h"
#include "vmath.h"
#include "bu/cmdschema.h"
#include "bu/file.h"
#include "bv/plot3.h"

struct plot3_args {
    int help;
    int binary_mode;
    int text_mode;
    int expect_invalid;
};

static const struct bu_cmd_option plot3_options[] = {
    BU_CMD_FLAG("h", "help", struct plot3_args, help, "Print help and exit"),
    BU_CMD_FLAG("b", "binary", struct plot3_args, binary_mode,
	"Process binary plot data (the default)"),
    BU_CMD_FLAG("t", "text", struct plot3_args, text_mode,
	"Process text plot data"),
    BU_CMD_FLAG("i", "invalid", struct plot3_args, expect_invalid,
	"Expect to detect an invalid file"),
    BU_CMD_OPTION_NULL
};

static const struct bu_cmd_operand plot3_operands[] = {
    BU_CMD_OPERAND("file", BU_CMD_VALUE_FILE, 1, 1, "Plot file", NULL),
    BU_CMD_OPERAND_NULL
};

static const char *plot3_mode_options[] = {"binary", "text", NULL};
static const struct bu_cmd_constraint plot3_constraints[] = {
    BU_CMD_CONSTRAINT_OPTIONS(plot3_mode_options, 0, 1,
	"specify either --binary or --text"),
    BU_CMD_CONSTRAINT_NULL
};

static const struct bu_cmd_schema plot3_schema = {
    "bview_plot3", "Check a binary or text plot file for invalid records",
    plot3_options, plot3_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, plot3_constraints)
};

int
main(int argc, const char *argv[])
{
    int ret = 0;
    struct plot3_args args = {0, 0, 0, 0};
    int mode = PL_OUTPUT_MODE_BINARY;

    bu_setprogname(argv[0]);

    argc-=(argc>0); argv+=(argc>0); /* done with command name argv[0] */

    int help_requested = bu_cmd_schema_option_present(&plot3_schema,
	(size_t)argc, argv, "help");
    int operand_index = help_requested ?
	bu_cmd_schema_parse(&plot3_schema, &args, NULL, argc, argv) :
	bu_cmd_schema_parse_complete(&plot3_schema, &args, NULL, argc, argv);

    if (args.help) {
	char *help = bu_cmd_schema_describe(&plot3_schema);
	bu_log("Usage: bview_plot3 [options] file\n%s", help ? help : "");
	if (help)
	    bu_free(help, "plot3 native help");
	return 0;
    }
    if (operand_index < 0)
	bu_exit(1, "Usage: bview_plot3 [options] file\n");

    argc -= operand_index;
    argv += operand_index;


    if (args.binary_mode)
	mode = PL_OUTPUT_MODE_BINARY;

    if (args.text_mode)
	mode = PL_OUTPUT_MODE_TEXT;

    if (!bu_file_exists(argv[0], NULL)) {
	bu_exit(1, "file %s not found\n", argv[0]);
    }

    FILE *fp = fopen(argv[0], "rb");

    /* A non-readable file isn't a valid file */
    if (!fp)
	bu_exit(1, "Error - could not open file %s\n", argv[0]);

    ret = plot3_invalid(fp, mode);

    if (args.expect_invalid) {
	if (ret) {
	    bu_log("INVALID (expected): %s\n", argv[0]);
	} else {
	    bu_log("VALID (unexpected): %s\n", argv[0]);
	}
	ret = (ret) ? 0 : 1;
    } else {
	if (ret) {
	    bu_log("INVALID: %s\n", argv[0]);
	} else {
	    bu_log("VALID:   %s\n", argv[0]);
	}
    }

    return ret;
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
