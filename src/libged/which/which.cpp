/*                         W H I C H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2020 United States Government as represented by
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

#include <string>
#include <set>
#include <map>
#include <algorithm>
#include <iterator>

#include "bu/cmd.h"
#include "bu/opt.h"
#include "bu/vls.h"

#include "../alphanum.h"
#include "../ged_private.h"

bool alphanum_cmp(const std::string& a, const std::string& b) {
    return alphanum_impl(a.c_str(), b.c_str(), NULL) < 0;
}

extern "C" int
ged_which_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    int isAir;
    int sflag = 0;
    int eflag = 0;
    int print_help = 0;
    int unused = 0;
    struct bu_vls root = BU_VLS_INIT_ZERO;
    struct bu_vls usage = BU_VLS_INIT_ZERO;
    const char *usageAir = "[options] code(s)";
    const char *usageIds = "[options] region_id(s)";
    std::map<int, std::set<std::string>> id2names;

    struct bu_opt_desc d[7];
    BU_OPT(d[0], "h", "help",      "",             NULL,        &print_help,   "Print help and exit");
    BU_OPT(d[1], "?", "",           "",            NULL,        &print_help,    "");
    BU_OPT(d[2], "s", "script",    "",             NULL,        &sflag,        "Different output formatting for scripting");
    BU_OPT(d[3], "V", "",          "",             NULL,        &eflag,        "List all active ids, even if no associated regions are found");
    BU_OPT(d[4], "U", "unused",    "",             NULL,        &unused,       "Report unused ids in the specified range");
    BU_OPT(d[5], "",  "root",      "<root_name>",  &bu_opt_vls, &root,         "Search only in the tree below 'root_name'");
    BU_OPT_NULL(d[6]);

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* TODO - this isn't a great pattern to follow... whichair should call
     * which with an option... */
    /* Key off of the command name to set the air option */
    isAir = (BU_STR_EQUAL(argv[0], "whichair")) ? 1 : 0;
    bu_vls_sprintf(&usage, "Usage: %s %s", argv[0], ((BU_STR_EQUAL(argv[0], "whichair")) ? usageAir : usageIds));

    argc-=(argc>0); argv+=(argc>0); /* done with command name argv[0] */

    if (!argc) {
	/* must be wanting help */
	_ged_cmd_help(gedp, bu_vls_cstr(&usage), d);
	bu_vls_free(&usage);
	bu_vls_free(&root);
	return GED_HELP;
    }

    /* parse standard options */
    int opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (print_help) {
	_ged_cmd_help(gedp, bu_vls_cstr(&usage), d);
	bu_vls_free(&usage);
	bu_vls_free(&root);
	return GED_OK;
    }

    /* adjust argc to match the leftovers of the options parsing */
    argc = opt_ret;


    if (!argc) {
	_ged_cmd_help(gedp, bu_vls_cstr(&usage), d);
	bu_vls_free(&usage);
	bu_vls_free(&root);
	return GED_ERROR;
    }

    bu_vls_free(&usage);

    std::set<int> ids;

    /* Build set of ids */
    for (int j = 0; j < argc; j++) {
	int n;
	int start, end;
	int range;
	int k;

	n = sscanf(argv[j], "%d%*[:-]%d", &start, &end);
	switch (n) {
	    case 1:
		ids.insert(start);
		break;
	    case 2:
		if (start < end)
		    range = end - start + 1;
		else if (end < start) {
		    range = start - end + 1;
		    start = end;
		} else {
		    ids.insert(start);
		    break;
		}
		for (k = 0; k < range; ++k) {
		    ids.insert(start + k);
		}
		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "Error: invalid range specification \"%s\"", argv[j]);
		bu_vls_free(&root);
		return GED_ERROR;
	}
    }

    /* Examine all region nodes */
    if (bu_vls_strlen(&root)) {
	// Find all regions in the specified root
	const char *sstring = "-type region";
	struct directory *sdp = db_lookup(gedp->ged_wdbp->dbip, bu_vls_cstr(&root), LOOKUP_QUIET);
	if (sdp == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "Error: no object named %s in database.", bu_vls_cstr(&root));
	    bu_vls_free(&root);
	    return GED_ERROR;
	}
	struct bu_ptbl comb_objs = BU_PTBL_INIT_ZERO;
	(void)db_search(&comb_objs, DB_SEARCH_TREE|DB_SEARCH_RETURN_UNIQ_DP, sstring, 1, &sdp, gedp->ged_wdbp->dbip, NULL);
	for(size_t i = 0; i < BU_PTBL_LEN(&comb_objs); i++) {
	    dp = (struct directory *)BU_PTBL_GET(&comb_objs, i);

	    if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
		bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
		bu_vls_free(&root);
		return GED_ERROR;
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
	for (int i = 0; i < RT_DBNHASH; i++) {
	    for (dp = gedp->ged_wdbp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
		if (!(dp->d_flags & RT_DIR_REGION))
		    continue;

		if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
		    bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
		    bu_vls_free(&root);
		    return GED_ERROR;
		}
		comb = (struct rt_comb_internal *)intern.idb_ptr;
		/* check to see if the region id or air code matches one in our list */
		int id = (isAir) ? comb->aircode : comb->region_id;
		if (ids.find(id) != ids.end()) {
		    id2names[id].insert(std::string(dp->d_namep));
		}

		rt_db_free_internal(&intern);
	    }
	}
    }

    /* report results */
    if (unused) {
	std::set<int> unused_ids;
	std::set<int>::iterator i_it;
	for (i_it = ids.begin(); i_it != ids.end(); i_it++) {
	    if (id2names.find(*i_it) != id2names.end() && id2names[*i_it].size() > 0) {
		continue;
	    }
	    unused_ids.insert(*i_it);
	}
	if (!sflag) {
	    if (unused_ids.size()) {
		bu_vls_printf(gedp->ged_result_str, "Unused %s:\n", isAir ? "air codes" : "idents");
	    } else {
		bu_vls_printf(gedp->ged_result_str, "No unused %s found\n", isAir ? "air codes" : "idents");
		bu_vls_free(&root);
		return GED_OK;
	    }
	}
	if (sflag) {
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
	bu_vls_free(&root);
	return GED_OK;
    }

    std::set<int>::iterator i_it;
    for (i_it = ids.begin(); i_it != ids.end(); i_it++) {
	std::map<int, std::set<std::string>>::iterator idn_it = id2names.find(*i_it);
	if ((eflag || (idn_it != id2names.end() && id2names[*i_it].size())) && !sflag) {
	    bu_vls_printf(gedp->ged_result_str, "Region[s] with %s %d:\n", isAir ? "air code" : "ident", *i_it);
	}
	if (idn_it == id2names.end()) {
	    continue;
	}
	std::vector<std::string> nsorted;
	std::copy(idn_it->second.begin(), idn_it->second.end(), std::back_inserter(nsorted));
	std::sort(nsorted.begin(), nsorted.end(), alphanum_cmp);
	for (size_t i = 0; i < nsorted.size(); i++) {
	    if (sflag) {
		bu_vls_printf(gedp->ged_result_str, " %s", nsorted[i].c_str());
	    } else {
		bu_vls_printf(gedp->ged_result_str, "   %s\n", nsorted[i].c_str());
	    }
	}
    }

    bu_vls_free(&root);
    return GED_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
extern "C" {
    struct ged_cmd_impl which_cmd_impl = { "which", ged_which_core, GED_CMD_DEFAULT };
    const struct ged_cmd which_cmd = { &which_cmd_impl };

    struct ged_cmd_impl whichair_cmd_impl = { "whichair", ged_which_core, GED_CMD_DEFAULT };
    const struct ged_cmd whichair_cmd = { &whichair_cmd_impl };

    struct ged_cmd_impl whichid_cmd_impl = { "whichid", ged_which_core, GED_CMD_DEFAULT };
    const struct ged_cmd whichid_cmd = { &whichid_cmd_impl };


    const struct ged_cmd *which_cmds[] = { &which_cmd,  &whichair_cmd, &whichid_cmd, NULL };

    static const struct ged_plugin pinfo = { GED_API,  which_cmds, 3 };

    COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
    {
	return &pinfo;
    }
}
#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
