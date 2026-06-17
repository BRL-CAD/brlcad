/*                       A T T R . C P P
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
/** @file libged/attr.cpp
 *
 * The attr command.
 *
 */

#include "common.h"

#include <algorithm>
#include <utility>
#include <string>
#include <set>
#include <cstdlib>
#include <vector>

#include "bu/getopt.h"
#include "rt/cmd.h"
#include "ged/database.h"
#include "ged/event_txn.h"

static int
attr_subcommand_mutates(const char *subcmd)
{
    return BU_STR_EQUAL(subcmd, "set") ||
	BU_STR_EQUAL(subcmd, "rm") ||
	BU_STR_EQUAL(subcmd, "append") ||
	BU_STR_EQUAL(subcmd, "copy") ||
	BU_STR_EQUAL(subcmd, "standardize");
}

static int
ged_attr_mutation_object_arg(int argc, const char *argv[])
{
    int opt;

    bu_optind = 1;
    while ((opt = bu_getopt(argc, (char * const *)argv, "c:")) != -1) {
	if (opt != 'c')
	    return -1;
    }

    argc -= bu_optind - 1;
    argv += bu_optind - 1;
    if (argc < 3 || !attr_subcommand_mutates(argv[1]))
	return -1;

    return bu_optind + 1;
}

static void
ged_attr_collect_mutation_targets(struct ged *gedp, const char *object_pattern,
	std::vector<std::string> &object_names)
{
    if (!gedp || !gedp->dbip || !object_pattern || !object_pattern[0])
	return;

    struct directory **paths = NULL;
    size_t path_cnt = db_ls(gedp->dbip, DB_LS_HIDDEN, object_pattern, &paths);
    if (!path_cnt || !paths) {
	if (paths)
	    bu_free(paths, "ged attr mutation paths");
	return;
    }

    for (size_t i = 0; i < path_cnt; i++) {
	if (!paths[i] || !paths[i]->d_namep)
	    continue;
	object_names.push_back(paths[i]->d_namep);
    }

    std::sort(object_names.begin(), object_names.end());
    object_names.erase(std::unique(object_names.begin(), object_names.end()),
	    object_names.end());

    bu_free(paths, "ged attr mutation paths");
}


static void
ged_attr_queue_mutation_events(struct ged *gedp,
			       const std::vector<std::string> &object_names)
{
    if (!gedp || object_names.empty())
	return;

    for (const std::string &object_name : object_names)
	ged_event_notify_attribute_changed(gedp, object_name.c_str(), 1,
		NULL);
}

extern "C" int
ged_attr_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "{[-c sep_char] set|get|show|rm|append|sort|list|copy|standardize} object [key [value] ... ]";
    const char *cmd_name = argv[0];

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
	return GED_HELP;
    }

    /* Verify that this wdb supports lookup operations
       (non-null dbip) */
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);

    int mutation_object_arg = ged_attr_mutation_object_arg(argc, argv);
    std::vector<std::string> mutation_targets;
    if (mutation_object_arg >= 0)
	ged_attr_collect_mutation_targets(gedp, argv[mutation_object_arg],
		mutation_targets);

    int mutation_event_batch_started = 0;
    if (mutation_object_arg >= 0)
	mutation_event_batch_started = (ged_event_batch_begin(gedp) > 0);

    int ret = rt_cmd_attr(gedp->ged_result_str, gedp->dbip, argc, argv);

    if (ret & GED_HELP) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
    }

    if (!(ret & BRLCAD_ERROR))
	ged_attr_queue_mutation_events(gedp, mutation_targets);

    if (mutation_event_batch_started) {
	struct ged_event_txn_result result;
	ged_event_txn_result_init(&result);
	(void)ged_event_batch_end(gedp, &result);
	ged_event_txn_result_free(&result);
    }

    if (ret & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    return ret;
}

#include "../include/plugin.h"

#define GED_ATTR_COMMANDS(X, XID) \
    X(attr, ged_attr_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_ATTR_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_attr", 1, GED_ATTR_COMMANDS)

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
