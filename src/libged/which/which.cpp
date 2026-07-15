/*                         W H I C H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/which.c
 *
 * The whichair and whichid commands.
 *
 */

#include "common.h"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <string>
#include <set>
#include <map>
#include <algorithm>
#include <iterator>

#include "bu/cmd.h"
#include "bu/cmdschema.h"
#include "bu/vls.h"

#include "../alphanum.h"
#include "../ged_private.h"

static bool
alphanum_cmp(const std::string& a, const std::string& b) {
    return alphanum_impl(a.c_str(), b.c_str(), NULL) < 0;
}

struct which_args {
    int print_help;
    int script;
    int all_active;
    int unused;
    const char *root;
};


static int
which_parse_range(struct bu_vls *msg, const char *arg, int *start_out, int *end_out)
{
    char *end = NULL;
    long start = 0;
    long finish = 0;

    if (!arg || !arg[0])
	goto invalid;
    errno = 0;
    start = std::strtol(arg, &end, 10);
    if (errno == ERANGE || !end || end == arg || start < INT_MIN || start > INT_MAX)
	goto invalid;
    if (!end[0]) {
	finish = start;
	} else {
	if (*end != '-' && *end != ':')
	    goto invalid;
	const char *range_end = end + 1;
	char *tail = NULL;
	errno = 0;
	finish = std::strtol(range_end, &tail, 10);
	if (errno == ERANGE || !tail || tail == range_end || *tail ||
		finish < INT_MIN || finish > INT_MAX)
	    goto invalid;
	}
    if (start_out)
	*start_out = (int)start;
    if (end_out)
	*end_out = (int)finish;
    return 0;

invalid:
    if (msg)
	bu_vls_printf(msg, "invalid integer or inclusive integer range: %s\n", arg ? arg : "");
    return -1;
}


static int
which_range_validate(struct bu_vls *msg, const char *arg)
{
    return which_parse_range(msg, arg, NULL, NULL);
}


static const struct bu_cmd_option which_options[] = {
    BU_CMD_FLAG("h", "help", struct which_args, print_help, "Print help and exit"),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_FLAG("s", "script", struct which_args, script,
	"Use script-oriented output"),
    BU_CMD_FLAG("V", NULL, struct which_args, all_active,
	"List requested active IDs even without matching regions"),
    BU_CMD_FLAG("U", "unused", struct which_args, unused,
	"Report unused IDs in the requested ranges"),
    BU_CMD_DB_OBJECT(NULL, "root", struct which_args, root, "object",
	"Restrict the search below a database object"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_arg_shape which_range_shape =
    BU_CMD_ARG_SHAPE(BU_CMD_ARG_SHAPE_RANGE_PATTERN, 1, 1,
	"Integer or inclusive integer range (start-end or start:end)");
static const struct bu_cmd_operand which_operands[] = {
    BU_CMD_OPERAND_SHAPED("id_range", BU_CMD_VALUE_STRING, 1,
	BU_CMD_COUNT_UNLIMITED, which_range_validate,
	"Region ID or air-code, or inclusive start-end/start:end range", NULL,
	&which_range_shape),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema which_cmd_schema = {
    "which", "Find regions with specified region IDs", which_options,
    which_operands, BU_CMD_PARSE_INTERSPERSED, {NULL}
};
static const struct bu_cmd_schema whichid_cmd_schema = {
    "whichid", "Find regions with specified region IDs", which_options,
    which_operands, BU_CMD_PARSE_INTERSPERSED, {NULL}
};
static const struct bu_cmd_schema whichair_cmd_schema = {
    "whichair", "Find regions with specified air codes", which_options,
    which_operands, BU_CMD_PARSE_INTERSPERSED, {NULL}
};


static void
which_show_help(struct ged *gedp, const char *command, const struct bu_cmd_schema *schema)
{
    char *option_help = bu_cmd_schema_describe(schema);

    bu_vls_printf(gedp->ged_result_str, "Usage: %s [options] %s\n", command,
	BU_STR_EQUAL(command, "whichair") ? "code(s)" : "region_id(s)");
    if (option_help) {
	bu_vls_printf(gedp->ged_result_str, "Options:\n%s", option_help);
	bu_free(option_help, "which native option help");
    }
}

extern "C" int
ged_which_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    int isAir;
    struct which_args options = {};
    const struct bu_cmd_schema *schema = NULL;
    struct bu_cmd_validate_result validation = BU_CMD_VALIDATE_RESULT_NULL;
    int operand_index = 0;
    int operand_count = 0;
    std::map<int, std::set<std::string>> id2names;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* TODO - this isn't a great pattern to follow... whichair should call
     * which with an option... */
    /* Key off of the command name to set the air option */
    isAir = (BU_STR_EQUAL(argv[0], "whichair")) ? 1 : 0;

    schema = isAir ? &whichair_cmd_schema :
	(BU_STR_EQUAL(argv[0], "whichid") ? &whichid_cmd_schema : &which_cmd_schema);

    if (argc == 1) {
	/* must be wanting help */
	which_show_help(gedp, argv[0], schema);
	return GED_HELP;
    }

    operand_index = bu_cmd_schema_parse(schema, &options, gedp->ged_result_str,
	argc - 1, argv + 1);
    if (operand_index < 0) {
	which_show_help(gedp, argv[0], schema);
	return BRLCAD_ERROR;
    }
    operand_count = argc - 1 - operand_index;


    if (options.print_help) {
	which_show_help(gedp, argv[0], schema);
	return BRLCAD_OK;
    }


    if (bu_cmd_schema_validate(schema, (size_t)(argc - 1), argv + 1,
	    (size_t)(argc - 1), &validation) != 0 ||
	validation.state != BU_CMD_VALIDATE_VALID) {
	if (validation.hint && validation.hint[0])
	    bu_vls_printf(gedp->ged_result_str, "%s\n", validation.hint);
	bu_cmd_validate_result_clear(&validation);
	which_show_help(gedp, argv[0], schema);
	return BRLCAD_ERROR;
    }
    bu_cmd_validate_result_clear(&validation);
    argv += operand_index + 1;
    argc = operand_count;

    std::set<int> ids;

    /* Build set of ids */
    for (int j = 0; j < argc; j++) {
	int start, end;

	if (which_parse_range(NULL, argv[j], &start, &end) != 0) {
	    bu_vls_printf(gedp->ged_result_str, "Error: invalid range specification \"%s\"", argv[j]);
	    return BRLCAD_ERROR;
	}
	if (start > end)
	    std::swap(start, end);
	for (long long id = start; id <= end; id++)
	    ids.insert((int)id);
    }

    /* Examine all region nodes */
    if (options.root) {
	// Find all regions in the specified root
	const char *sstring = "-type region";
	struct directory *sdp = db_lookup(gedp->dbip, options.root, LOOKUP_QUIET);
	if (sdp == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "Error: no object named %s in database.", options.root);
	    return BRLCAD_ERROR;
	}
	struct bu_ptbl comb_objs = BU_PTBL_INIT_ZERO;
	(void)db_search(&comb_objs, DB_SEARCH_TREE|DB_SEARCH_RETURN_UNIQ_DP, sstring, 1, &sdp, gedp->dbip, NULL, NULL, NULL);
	for(size_t i = 0; i < BU_PTBL_LEN(&comb_objs); i++) {
	    dp = (struct directory *)BU_PTBL_GET(&comb_objs, i);

	    if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL) < 0) {
		bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
		return BRLCAD_ERROR;
	    }
	    comb = (struct rt_comb_internal *)intern.idb_ptr;
	    /* check to see if the region id or air code matches one in our list */
	    int id = (isAir) ? comb->aircode : comb->region_id;
	    if (ids.find(id) != ids.end()) {
		id2names[id].insert(std::string(dp->d_namep));
	    }

	    rt_db_free_internal(&intern);
	}
	db_search_free(&comb_objs);
    } else {
	FOR_ALL_DIRECTORY_START(dp, gedp->dbip)
	    if (!(dp->d_flags & RT_DIR_REGION))
		continue;

	    if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL) < 0) {
		bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
		return BRLCAD_ERROR;
	    }
	    comb = (struct rt_comb_internal *)intern.idb_ptr;
	    /* check to see if the region id or air code matches one in our list */
	    int id = (isAir) ? comb->aircode : comb->region_id;
	    if (ids.find(id) != ids.end()) {
		id2names[id].insert(std::string(dp->d_namep));
	    }

	    rt_db_free_internal(&intern);
	FOR_ALL_DIRECTORY_END;
    }

    /* report results */
    if (options.unused) {
	std::set<int> unused_ids;
	std::set<int>::iterator i_it;
	for (i_it = ids.begin(); i_it != ids.end(); i_it++) {
	    if (id2names.find(*i_it) != id2names.end() && id2names[*i_it].size() > 0) {
		continue;
	    }
	    unused_ids.insert(*i_it);
	}
	if (!options.script) {
	    if (unused_ids.size()) {
		bu_vls_printf(gedp->ged_result_str, "Unused %s:\n", isAir ? "air codes" : "idents");
	    } else {
		bu_vls_printf(gedp->ged_result_str, "No unused %s found\n", isAir ? "air codes" : "idents");
		return BRLCAD_OK;
	    }
	}
	if (options.script) {
	    for (i_it = unused_ids.begin(); i_it != unused_ids.end(); i_it++) {
		bu_vls_printf(gedp->ged_result_str, "   %d", *i_it);
	    }
	} else {
	    // To reduce verbosity, assemble and print ranges of unused numbers rather
	    // that just dumping all of them
	    bool have_rstart = false;
	    bool have_rend = false;
	    int rstart = -INT_MAX;
	    int rend = -INT_MAX;
	    int rprev = -INT_MAX;
	    for (i_it = unused_ids.begin(); i_it != unused_ids.end(); i_it++) {
		if (!have_rstart || (*i_it != rprev+1)) {
		    // Print intermediate results, if we find a sequence within
		    // the overall results.
		    if (have_rstart) {
			if (have_rend) {
			    bu_vls_printf(gedp->ged_result_str, "   %d-%d\n", rstart, rend);
			} else {
			    bu_vls_printf(gedp->ged_result_str, "   %d\n", rstart);
			}
			have_rend = false;
			rend = -INT_MAX;
		    }
		    have_rstart = true;
		    rstart = *i_it;
		    rprev = *i_it;
		    continue;
		}
		rprev = *i_it;
		rend = *i_it;
		have_rend = true;
		continue;
	    }
	    // Print the last results
	    if (have_rend) {
		bu_vls_printf(gedp->ged_result_str, "   %d-%d\n", rstart, rend);
	    } else {
		bu_vls_printf(gedp->ged_result_str, "   %d\n", rstart);
	    }
	}
	return BRLCAD_OK;
    }

    std::set<int>::iterator i_it;
    for (i_it = ids.begin(); i_it != ids.end(); i_it++) {
	std::map<int, std::set<std::string>>::iterator idn_it = id2names.find(*i_it);
	if ((options.all_active || (idn_it != id2names.end() && id2names[*i_it].size())) && !options.script) {
	    bu_vls_printf(gedp->ged_result_str, "Region[s] with %s %d:\n", isAir ? "air code" : "ident", *i_it);
	}
	if (idn_it == id2names.end()) {
	    continue;
	}
	std::vector<std::string> nsorted;
	std::copy(idn_it->second.begin(), idn_it->second.end(), std::back_inserter(nsorted));
	std::sort(nsorted.begin(), nsorted.end(), alphanum_cmp);
	for (size_t i = 0; i < nsorted.size(); i++) {
	    if (options.script) {
		bu_vls_printf(gedp->ged_result_str, " %s", nsorted[i].c_str());
	    } else {
		bu_vls_printf(gedp->ged_result_str, "   %s\n", nsorted[i].c_str());
	    }
	}
    }

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_WHICH_COMMANDS(X, XID) \
    X(which, ged_which_core, GED_CMD_DEFAULT, &which_cmd_schema) \
    X(whichair, ged_which_core, GED_CMD_DEFAULT, &whichair_cmd_schema) \
    X(whichid, ged_which_core, GED_CMD_DEFAULT, &whichid_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_WHICH_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_which", 1, GED_WHICH_COMMANDS)

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
