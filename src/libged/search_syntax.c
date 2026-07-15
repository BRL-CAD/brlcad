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


static const struct ged_search_top_option search_top_options[] = {
    {"-a", GED_SEARCH_TOP_OPTION_HIDDEN},
    {"-Q", GED_SEARCH_TOP_OPTION_QUIET},
    {"-h", GED_SEARCH_TOP_OPTION_HELP},
    {"-?", GED_SEARCH_TOP_OPTION_HELP},
    {"-v", GED_SEARCH_TOP_OPTION_VERBOSE}
};


const struct ged_search_top_option *
ged_search_top_options(size_t *count)
{
    if (count)
	*count = sizeof(search_top_options) / sizeof(search_top_options[0]);
    return search_top_options;
}


int
ged_search_top_option_parse(const char *word, enum ged_search_top_option_kind *kind,
	size_t *occurrences)
{
    size_t count = 0;
    const struct ged_search_top_option *options = ged_search_top_options(&count);

    if (kind)
	*kind = GED_SEARCH_TOP_OPTION_UNKNOWN;
    if (occurrences)
	*occurrences = 0;
    if (!word || word[0] != '-' || !word[1])
	return 0;

    for (size_t i = 0; i < count; i++) {
	if (options[i].kind != GED_SEARCH_TOP_OPTION_VERBOSE &&
		BU_STR_EQUAL(word, options[i].name)) {
	    if (kind)
		*kind = options[i].kind;
	    if (occurrences)
		*occurrences = 1;
	    return 1;
	}
    }

    if (word[1] != 'v')
	return 0;
    for (size_t i = 1; word[i]; i++) {
	if (word[i] != 'v')
	    return 0;
    }

    if (kind)
	*kind = GED_SEARCH_TOP_OPTION_VERBOSE;
    if (occurrences)
	*occurrences = strlen(word) - 1;
    return 1;
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
