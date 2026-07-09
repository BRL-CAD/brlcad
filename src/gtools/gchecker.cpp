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
#include <map>
#include <set>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/opt.h"
#include "bu/path.h"
#include "analyze.h"
#include "ged.h"

static void
_cmd_help(const char *usage, struct bu_opt_desc *d)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;
    char *option_help;

    bu_vls_sprintf(&str, "%s", usage);

    if ((option_help = bu_opt_describe(d, NULL))) {
        bu_vls_printf(&str, "Options:\n%s\n", option_help);
        bu_free(option_help, "help str");
    }

    bu_log("%s", bu_vls_cstr(&str));
    bu_vls_free(&str);
}


int
main(int argc, const char **argv)
{
    int print_help = 0;
    int dry_run = 0;
    int verbose = 0;
    int force = 0;
    const char *usage = "Usage: gchecker [options] file.g  [objects ...]\n\n";
    struct bu_opt_desc d[5];
    BU_OPT(d[0], "d", "dry-run", "",  NULL, &dry_run,     "Step through the checker stages, but don't raytrace");
    BU_OPT(d[1], "v", "verbose", "",  NULL, &verbose,     "Print verbose information about result processing");
    BU_OPT(d[2], "f", "force",   "",  NULL, &force,       "Overwrite existing .ck directory");
    BU_OPT(d[3], "h", "help",    "",  NULL, &print_help,  "Print help and exit");
    BU_OPT_NULL(d[4]);

    std::set<std::pair<std::string, std::string>> unique_pairs;
    std::multimap<std::pair<std::string, std::string>, double> pair_sizes;
    std::map<std::pair<std::string, std::string>, double> pair_avg_sizes;


    bu_setprogname(argv[0]);

    argc-=(argc>0); argv+=(argc>0);

       /* must be wanting help */
    if (argc < 1) {
        _cmd_help(usage, d);
        return 0;
    }

    /* parse standard options */
    int opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (print_help) {
        _cmd_help(usage, d);
        return 0;
    }

    /* adjust argc to match the leftovers of the options parsing */
    argc = opt_ret;

    if (argc < 1) {
	_cmd_help(usage, d);
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
	if (force) {
	    bu_dirclear(bu_vls_cstr(&wdir));
	} else {
	    bu_vls_free(&gfile);
	    bu_vls_free(&gbasename);
	    bu_exit(1, "Working directory\"%s\" already exists - remove to continue", bu_vls_cstr(&wdir));
	}
    }


    if (dry_run) {
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

    if (verbose) {
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

    // Run overlap analysis via libanalyze for each top-level object.
    // Previously this was two nested loops (rtcheck equiv + gqa equiv)
    // that called `check overlaps` as a GED command and parsed the text
    // output with a regex.  Now we call analyze_run() directly to get
    // structured overlap records, which is faster, more reliable, and
    // avoids the regex text-parsing fragility.
    if (!dry_run) {
	for (size_t i = 0; i < objs.size(); i++) {
	    char *names[1];
	    names[0] = objs[i]->d_namep;

	    // First pass: high-resolution fixed-grid (analogous to old rtcheck equiv,
	    // 1024×1024 cells per view direction).
	    {
		struct analyze_config cfg = ANALYZE_CONFIG_INIT_ZERO;
		cfg.grid_width  = 1024;
		cfg.grid_height = 1024;
		cfg.quiet_missed = 1;
		struct analyze_results *res =
		    analyze_run(&cfg, gedp->dbip, names, 1, ANALYZE_OVERLAPS);
		if (!res) {
		    bu_exit(1, "error running analyze_run (high-res pass) on '%s'\n",
			   objs[i]->d_namep);
		}
		for (size_t k = 0; k < BU_PTBL_LEN(&res->overlaps); k++) {
		    struct analyze_overlap_record *ov =
			(struct analyze_overlap_record *)BU_PTBL_GET(&res->overlaps, k);
		    const std::string r1(ov->region1 ? ov->region1 : "");
		    const std::string r2(ov->region2 ? ov->region2 : "");
		    std::pair<std::string, std::string> key =
			(r1 < r2) ? std::make_pair(r1, r2) : std::make_pair(r2, r1);
		    double val = (double)ov->count * ov->max_dist;
		    unique_pairs.insert(key);
		    pair_sizes.insert(std::make_pair(key, val));
		    if (verbose) {
			bu_log("Inserting (hi-res): %s,%s -> %f\n",
			       key.first.c_str(), key.second.c_str(), val);
		    }
		}
		analyze_results_free(res);
	    }

	    // Second pass: fine uniform grid spacing (analogous to old gqa equiv,
	    // 1 mm grid spacing).
	    {
		struct analyze_config cfg = ANALYZE_CONFIG_INIT_ZERO;
		cfg.grid_spacing     = 1.0; /* 1 mm initial */
		cfg.grid_spacing_min = 1.0; /* 1 mm limit   */
		cfg.quiet_missed = 1;
		struct analyze_results *res =
		    analyze_run(&cfg, gedp->dbip, names, 1, ANALYZE_OVERLAPS);
		if (!res) {
		    bu_exit(1, "error running analyze_run (fine-grid pass) on '%s'\n",
			   objs[i]->d_namep);
		}
		for (size_t k = 0; k < BU_PTBL_LEN(&res->overlaps); k++) {
		    struct analyze_overlap_record *ov =
			(struct analyze_overlap_record *)BU_PTBL_GET(&res->overlaps, k);
		    const std::string r1(ov->region1 ? ov->region1 : "");
		    const std::string r2(ov->region2 ? ov->region2 : "");
		    std::pair<std::string, std::string> key =
			(r1 < r2) ? std::make_pair(r1, r2) : std::make_pair(r2, r1);
		    double val = (double)ov->count * ov->max_dist;
		    unique_pairs.insert(key);
		    pair_sizes.insert(std::make_pair(key, val));
		    if (verbose) {
			bu_log("Inserting (fine-grid): %s,%s -> %f\n",
			       key.first.c_str(), key.second.c_str(), val);
		    }
		}
		analyze_results_free(res);
	    }
	}
    }

    if (verbose) {
	bu_log("Found %zd unique pairings: \n", unique_pairs.size());
    }
    std::set<std::pair<std::string, std::string>>::iterator p_it;
    for (p_it = unique_pairs.begin(); p_it != unique_pairs.end(); p_it++) {
	if (verbose) {
	    bu_log("     %s + %s: \n", p_it->first.c_str(), p_it->second.c_str());
	}
	// For each pairing, get the average size
	size_t scnt = pair_sizes.count(*p_it);
	double ssum = 0.0;
	if (verbose) {
	    bu_log("     Have %zd sizes: \n", scnt);
	}
	std::multimap<std::pair<std::string, std::string>, double>::iterator s_it;
	for (s_it = pair_sizes.equal_range(*p_it).first; s_it != pair_sizes.equal_range(*p_it).second; s_it++) {
	    double s = (*s_it).second;
	    ssum += s;
	    if (verbose) {
		bu_log("                   %f \n", s);
	    }
	}
	if (verbose) {
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
