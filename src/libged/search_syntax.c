/*                      S E A R C H _ S Y N T A X . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1 as
 * published by the Free Software Foundation.
 */
/** @file libged/search_syntax.c
 *
 * Shared vocabulary for the GED-specific prefix of the search command.
 */

#include "common.h"

#include <string.h>

#include "ged_private.h"


static const struct bu_arg_option search_top_options[] = {
    BU_ARG_FLAG("a", NULL, struct ged_search_top_args, hidden, "Include hidden database objects"),
    BU_ARG_FLAG("Q", NULL, struct ged_search_top_args, quiet, "Suppress search diagnostics"),
    BU_ARG_FLAG("h", NULL, struct ged_search_top_args, help, "Print command help"),
    BU_ARG_FLAG("?", NULL, struct ged_search_top_args, help, "Print command help"),
    BU_ARG_COUNTING_FLAG("v", NULL, struct ged_search_top_args, verbosity,
	BU_ARG_VALUE_LONG, "Increase output detail; repeat up to three levels"),
    BU_ARG_OPTION_NULL
};

const struct bu_arg_schema ged_search_top_schema = {
    "search", "Search database objects", search_top_options, NULL,
    BU_ARG_PARSE_OPTIONS_FIRST, BU_ARG_SCHEMA_CONSTRAINTS(NULL, NULL)
};

int
ged_search_top_option_parse(const char *word, struct ged_search_top_args *args)
{
    const char *argv[] = {word};
    struct ged_search_top_args unused = {0};

    if (BU_STR_EQUAL(word, "--"))
	return 0;

    return bu_arg_schema_parse_known(&ged_search_top_schema,
	args ? args : &unused, NULL, 1, argv);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
