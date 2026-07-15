/*                    G C H E C K E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2026 United States Government as represented by
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
/** @file gchecker.cpp
 *
 * C++ version of the check.tcl logic generating overlap files used
 * by the MGED Overlap Tool
 *
 */

#include "common.h"

#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h> /* for mkdir */
#endif

#ifdef HAVE_WINDOWS_H
#  include <direct.h> /* For chdir */
#endif

#include <fstream>
#include <iostream>
#include <iomanip>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/cmdschema.h"
#include "bu/file.h"
#include "bu/path.h"
#include "ged.h"

struct gchecker_args {
    int dry_run;
    int verbose;
    int force;
    int help;
};

static const struct bu_cmd_option gchecker_options[] = {
    BU_CMD_FLAG("d", "dry-run", struct gchecker_args, dry_run,
	"Step through the checker stages, but do not raytrace"),
    BU_CMD_FLAG("v", "verbose", struct gchecker_args, verbose,
	"Print verbose information about result processing"),
    BU_CMD_FLAG("f", "force", struct gchecker_args, force,
	"Overwrite an existing .ck directory"),
    BU_CMD_FLAG("h", "help", struct gchecker_args, help,
	"Print help and exit"),
    BU_CMD_OPTION_NULL
};

static const struct bu_cmd_operand gchecker_operands[] = {
    BU_CMD_OPERAND("database", BU_CMD_VALUE_FILE, 1, 1,
	"Input BRL-CAD database", NULL),
    BU_CMD_OPERAND("object", BU_CMD_VALUE_STRING, 0, BU_CMD_COUNT_UNLIMITED,
	"Optional object name", NULL),
    BU_CMD_OPERAND_NULL
};

static const struct bu_cmd_schema gchecker_schema = {
    "gchecker", "Generate geometry-overlap data for the MGED Overlap Tool",
    gchecker_options, gchecker_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

static void
_cmd_help(const char *usage, const struct bu_cmd_schema *schema)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;
    char *option_help;

    bu_vls_sprintf(&str, "%s", usage);

    if ((option_help = bu_cmd_schema_describe(schema))) {
        bu_vls_printf(&str, "Options:\n%s\n", option_help);
        bu_free(option_help, "help str");
    }

    bu_log("%s", bu_vls_cstr(&str));
    bu_vls_free(&str);
}


int
main(int argc, const char **argv)
{
    struct gchecker_args args = {0, 0, 0, 0};
    const char *usage = "Usage: gchecker [options] file.g  [objects ...]\n\n";

    std::set<std::pair<std::string, std::string>> unique_pairs;
    std::multimap<std::pair<std::string, std::string>, double> pair_sizes;
    std::map<std::pair<std::string, std::string>, double> pair_avg_sizes;


    bu_setprogname(argv[0]);

    argc-=(argc>0); argv+=(argc>0);

       /* must be wanting help */
    if (argc < 1) {
        _cmd_help(usage, &gchecker_schema);
        return 0;
    }

    int help_requested = bu_cmd_schema_option_present(&gchecker_schema,
	(size_t)argc, argv, "help");
    int operand_index = help_requested ?
	bu_cmd_schema_parse(&gchecker_schema, &args, NULL, argc, argv) :
	bu_cmd_schema_parse_complete(&gchecker_schema, &args, NULL, argc, argv);

    if (args.help || operand_index < 0) {
        _cmd_help(usage, &gchecker_schema);
        return 0;
    }

    /* Advance to the contiguous operand suffix selected by the native parser. */
    argc -= operand_index;
    argv += operand_index;

    if (argc < 1) {
	_cmd_help(usage, &gchecker_schema);
	return 1;
    }

    // Have programs - see if we have the .g file
    struct bu_vls gfile = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&gfile, "%s", argv[0]);
    if (!bu_file_exists(bu_vls_cstr(&gfile), NULL)) {
	char fpgfile[MAXPATHLEN] = {0};
	bu_dir(fpgfile, MAXPATHLEN, BU_DIR_CURR, bu_vls_cstr(&gfile), NULL);
	bu_vls_sprintf(&gfile, "%s", fpgfile);
	if (!bu_file_exists(bu_vls_cstr(&gfile), NULL)) {
	    bu_vls_free(&gfile);
	    bu_exit(1, "file %s does not exist", argv[0]);
	}
    }

    struct bu_vls gbasename = BU_VLS_INIT_ZERO;
    if (!bu_path_component(&gbasename, bu_vls_cstr(&gfile), BU_PATH_BASENAME)) {
	bu_vls_free(&gfile);
	bu_exit(1, "Could not identify basename in geometry file path \"%s\"", argv[0]);
    }
    struct bu_vls wdir = BU_VLS_INIT_ZERO;
    bu_vls_printf(&wdir, "%s.ck", bu_vls_cstr(&gbasename));
    if (bu_file_exists(bu_vls_cstr(&wdir), NULL)) {
	if (args.force) {
	    bu_dirclear(bu_vls_cstr(&wdir));
	} else {
	    bu_vls_free(&gfile);
	    bu_vls_free(&gbasename);
	    bu_exit(1, "Working directory\"%s\" already exists - remove to continue", bu_vls_cstr(&wdir));
	}
    }


    if (args.dry_run) {
	bu_log("(Note: dry run - skipping rtcheck)\n");
    }

    // Make the working directory
    bu_mkdir(bu_vls_cstr(&wdir));

    // Put a copy of the .g file in the working directory
    std::ifstream sgfile(bu_vls_cstr(&gfile), std::ios::binary);
    std::ofstream dgfile;
    dgfile.open(bu_dir(NULL, 0, bu_vls_cstr(&wdir), bu_vls_cstr(&gbasename), NULL), std::ios::binary);
    dgfile << sgfile.rdbuf();
    dgfile.close();
    sgfile.close();

    // Change working directory
    if (chdir(bu_vls_cstr(&wdir))) {
	bu_exit(1, "Failed to chdir to \"%s\" ", bu_vls_cstr(&wdir));
    }

    if (args.verbose) {
	bu_log("Working on a copy in %s\n", bu_vls_cstr(&wdir));
    }

    // All set - open up the .g file and go to work.
    struct ged *gedp = ged_open("db", bu_vls_cstr(&gbasename), 1);
    if (gedp == GED_NULL) {
	bu_exit(1, "Failed to open \"%s\" ", bu_vls_cstr(&gbasename));
    }

    // Make sure our reference counts are up to date, so we can tell
    // which objects are top level
    db_update_nref(gedp->dbip);

    std::vector<struct directory *> objs;
    if (argc > 1) {
	for (int i = 1; i < argc; i++) {
	    struct directory *dp = db_lookup(gedp->dbip, argv[i], LOOKUP_QUIET);
	    if (dp == RT_DIR_NULL) {
		bu_exit(1, "Failed to open object \"%s\" in \"%s\" ", argv[i], bu_vls_cstr(&gbasename));
	    }
	    objs.push_back(dp);
	}
    } else {
	// Get all top level objects
	struct directory **all_paths;
	int obj_cnt = db_ls(gedp->dbip, DB_LS_TOPS, NULL, &all_paths);
	for (int i = 0; i < obj_cnt; i++) {
	    objs.push_back(all_paths[i]);
	}
	bu_free(all_paths, "free db_tops output");
    }

    if (objs.size() == 1) {
	bu_log("Processing tops object: %s\n", objs[0]->d_namep);
    } else {
	bu_log("Processing tops objects:\n");
	for (size_t i = 0; i < objs.size(); i++) {
	    bu_log("   %s\n", objs[i]->d_namep);
	}
    }

    int total_views = 0;
    for (size_t i = 0; i < objs.size(); i++) {
	for (int az = 0; az < 180; az+=45) {
	    for (int el = 0; el < 180; el+=45) {
		total_views++;
	    }
	}
    }

    if (total_views != (int)(16 * objs.size())) {
	bu_exit(1, "view incrementing error\n");
    }

    // Run rtcheck equiv.
    if (!args.dry_run) {
	std::regex oregex("<(.*),.(.*)>: ([0-9]*).* (.*).mm");
	for (size_t i = 0; i < objs.size(); i++) {
	    for (int az = 0; az < 180; az+=45) {
		for (int el = 0; el < 180; el+=45) {
		    struct bu_vls str = BU_VLS_INIT_ZERO;
		    const char **av = (const char **)bu_calloc(8, sizeof(char *), "cmd array");
		    av[0] = bu_strdup("check");
		    av[1] = bu_strdup("overlaps");
		    av[2] = bu_strdup("-G1024");
		    bu_vls_sprintf(&str, "-a%d", az);
		    av[3] = bu_strdup(bu_vls_cstr(&str));
		    bu_vls_sprintf(&str, "-e%d", el);
		    av[4] = bu_strdup(bu_vls_cstr(&str));
		    av[5] = bu_strdup("-q");
		    av[6] = bu_strdup(objs[i]->d_namep);
		    bu_vls_trunc(gedp->ged_result_str, 0);
		    if (ged_exec_check(gedp, 7, av) != BRLCAD_OK) {
			bu_exit(1, "error running ged 'check' command\n");
		    }
		    for (int j = 0; j < 7; j++) bu_free((void *)av[j], "str");
		    bu_free(av, "av array");
		    bu_vls_free(&str);

		    // Split up results into something we can process with regex
		    std::istringstream sres(bu_vls_cstr(gedp->ged_result_str));
		    std::string line;
		    while (std::getline(sres, line)) {
			std::smatch nvar;
			if (!std::regex_search(line, nvar, oregex) || nvar.size() != 5) {
			    continue;
			}
			if (args.verbose) {
			    bu_log("%zd: %s\n", nvar.size(), line.c_str());
			    for (size_t m = 0; m < nvar.size(); m++) {
				bu_log("   %zd: %s\n", m, nvar.str(m).c_str());
			    }
			}
			std::pair<std::string, std::string> key;
			// sort left and right strings lexicographically to produce unique pairing keys
			key = (nvar.str(1) < nvar.str(2)) ? std::make_pair(nvar.str(1), nvar.str(2)) :  std::make_pair(nvar.str(2), nvar.str(1));
			// size = count * depth
			double val = std::stod(nvar.str(3)) * std::stod(nvar.str(4));
			unique_pairs.insert(key);
			pair_sizes.insert(std::make_pair(key, val));
			if (args.verbose) {
			    bu_log("Inserting: %s,%s -> %f\n", key.first.c_str(), key.second.c_str(), val);
			}
		    }
		}
	    }
	}

	// Run gqa equiv.
	for (size_t i = 0; i < objs.size(); i++) {
	    for (int az = 0; az < 180; az+=45) {
		for (int el = 0; el < 180; el+=45) {
		    const char **av = (const char **)bu_calloc(6, sizeof(char *), "cmd array");
		    av[0] = bu_strdup("check");
		    av[1] = bu_strdup("overlaps");
		    av[2] = bu_strdup("-g1mm,1mm");
		    av[3] = bu_strdup("-q");
		    av[4] = bu_strdup(objs[i]->d_namep);
		    bu_vls_trunc(gedp->ged_result_str, 0);
		    if (ged_exec_check(gedp, 5, av) != BRLCAD_OK) {
			bu_exit(1, "error running ged 'check' command\n");
		    }
		    for (int j = 0; j < 5; j++) bu_free((void *)av[j], "str");
		    bu_free(av, "av array");

		    // Split up results into something we can process with regex
		    std::istringstream sres(bu_vls_cstr(gedp->ged_result_str));
		    std::string line;
		    while (std::getline(sres, line)) {
			std::smatch nvar;
			if (!std::regex_search(line, nvar, oregex) || nvar.size() != 5) {
			    continue;
			}
			if (args.verbose) {
			    bu_log("%zd: %s\n", nvar.size(), line.c_str());
			    for (size_t m = 0; m < nvar.size(); m++) {
				bu_log("   %zd: %s\n", m, nvar.str(m).c_str());
			    }
			}
			std::pair<std::string, std::string> key;
			// sort left and right strings lexicographically to produce unique pairing keys
			key = (nvar.str(1) < nvar.str(2)) ? std::make_pair(nvar.str(1), nvar.str(2)) :  std::make_pair(nvar.str(2), nvar.str(1));
			// size = count * depth
			double val = std::stod(nvar.str(3)) * std::stod(nvar.str(4));
			unique_pairs.insert(key);
			pair_sizes.insert(std::make_pair(key, val));
			if (args.verbose) {
			    bu_log("Inserting: %s,%s -> %f\n", key.first.c_str(), key.second.c_str(), val);
			}
		    }
		}
	    }
	}
    }

    if (args.verbose) {
	bu_log("Found %zd unique pairings: \n", unique_pairs.size());
    }
    std::set<std::pair<std::string, std::string>>::iterator p_it;
    for (p_it = unique_pairs.begin(); p_it != unique_pairs.end(); p_it++) {
	if (args.verbose) {
	    bu_log("     %s + %s: \n", p_it->first.c_str(), p_it->second.c_str());
	}
	// For each pairing, get the average size
	size_t scnt = pair_sizes.count(*p_it);
	double ssum = 0.0;
	if (args.verbose) {
	    bu_log("     Have %zd sizes: \n", scnt);
	}
	std::multimap<std::pair<std::string, std::string>, double>::iterator s_it;
	for (s_it = pair_sizes.equal_range(*p_it).first; s_it != pair_sizes.equal_range(*p_it).second; s_it++) {
	    double s = (*s_it).second;
	    ssum += s;
	    if (args.verbose) {
		bu_log("                   %f \n", s);
	    }
	}
	if (args.verbose) {
	    bu_log("     Avg: %f\n", ssum/(double)scnt);
	}
	pair_avg_sizes[*p_it] = ssum/(double)scnt;
    }

    // If we have something to write out, do so
    if (pair_avg_sizes.size()) {
	std::string ofile = std::string("ck.") + std::string(bu_vls_cstr(&gbasename)) + std::string(".overlaps");
	std::ofstream of(ofile);
	std::map<std::pair<std::string, std::string>, double>::iterator a_it;
	for (a_it = pair_avg_sizes.begin(); a_it != pair_avg_sizes.end(); a_it++) {
	    bu_log("%s + %s: %f\n", a_it->first.first.c_str(), a_it->first.second.c_str(), a_it->second);
	    of << a_it->first.first.c_str() << " " << a_it->first.second.c_str() << " " << std::fixed << std::setprecision(5) << a_it->second << "\n";
	}
	of.close();
    }

    // Remove the copy of the .g file
    ged_close(gedp);
    bu_file_delete(bu_vls_cstr(&gbasename));

    // Clean up
    bu_vls_free(&gfile);
    bu_vls_free(&gbasename);
    bu_vls_free(&wdir);

    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
