/*               G A R B A G E _ C O L L E C T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2023 United States Government as represented by
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
/** @file libged/garbage_collect.cpp
 *
 * The garbage_collect command.
 *
 */

#include "common.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/cmd.h"
#include "bu/opt.h"
#include "bu/path.h"
#include "raytrace.h"
#include "ged.h"

void print_help_msg(struct bu_vls *str)
{
    bu_vls_printf(str, "Usage: garbage_collect [-c|--confirm] [-h|--help]\n");
    bu_vls_printf(str, "\n");
    bu_vls_printf(str, "garbage_collect reclaims any available free space in the currently\n");
    bu_vls_printf(str, "open geometry database file.  As objects are deleted and created,\n");
    bu_vls_printf(str, "BRL-CAD geometry files can become fragmented and require cleanup.\n");
    bu_vls_printf(str, "\n");
    bu_vls_printf(str, "This command will automatically reclaim any available space by\n");
    bu_vls_printf(str, "keeping all existing top-level objects to a new file and, after\n");
    bu_vls_printf(str, "verifying that all objects were successfully saved, will replace\n");
    bu_vls_printf(str, "the currently open geometry database with the new file.\n");
    bu_vls_printf(str, "\n");
    bu_vls_printf(str, "DUE TO THE POTENTIAL FOR DATA CORRUPTION, PLEASE MANUALLY BACK UP\n");
    bu_vls_printf(str, "YOUR GEOMETRY FILE BEFORE RUNNING 'garbage_collect'.\n");
}

extern "C" int
ged_garbage_collect_core(struct ged *gedp, int argc, const char *argv[])
{
    const char *av[10] = {NULL};
    const char *ncmd = NULL;
    fastf_t fs_percent = 0.0;
    int confirmed = 0;
    int new_file_size = 0;
    int old_file_size = 0;
    int path_cnt = 0;
    int print_help = 0;
    int ret = BRLCAD_OK;
    int verify_failure = 0;
    std::set<std::string> missing_new_top_objs;
    std::set<std::string> missing_old_top_objs;
    std::set<std::string> new_top_objs;
    std::set<std::string> old_top_objs;
    std::set<std::string>::iterator s_it;
    std::vector<std::string> who_objs;
    struct bu_attribute_value_set avs;
    struct bu_vls bkup_file = BU_VLS_INIT_ZERO;
    struct bu_vls fdir = BU_VLS_INIT_ZERO;
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    struct bu_vls fpath = BU_VLS_INIT_ZERO;
    struct bu_vls working_file = BU_VLS_INIT_ZERO;
    struct db_i *old_dbip = NULL;
    struct directory **paths = NULL;
    struct directory *dp = NULL;
    struct directory *new_global_dp = NULL;
    struct directory *old_global_dp = NULL;
    struct rt_wdb *gc_wdbp = NULL;
    std::ifstream cfile;
    std::ofstream ofile;

    struct bu_opt_desc d[3];
    BU_OPT(d[0], "h", "help",      "",             NULL,        &print_help,   "Print help and exit");
    BU_OPT(d[1], "c", "confirm",   "",             NULL,        &confirmed,    "Execute garbage collect operation");
    BU_OPT_NULL(d[2]);

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    if (db_version(gedp->dbip) < 5) {
	bu_vls_printf(gedp->ged_result_str, "garbage_collect requires a v5 database\n");
	return BRLCAD_ERROR;
    }

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Done with command name argv[0] */
    argc-=(argc>0); argv+=(argc>0);

    if (!argc) {
	print_help_msg(gedp->ged_result_str);
	return GED_HELP;
    }

    /* parse standard options */
    int opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (print_help) {
	print_help_msg(gedp->ged_result_str);
	return BRLCAD_OK;
    }

    if (!confirmed || opt_ret) {
	bu_vls_printf(gedp->ged_result_str, "Usage: garbage_collect [-c|--confirm] [-h|--help]");
	return BRLCAD_ERROR;
    }

    /* See if we have a stale backup file */
    bu_path_component(&fdir, gedp->dbip->dbi_filename, BU_PATH_DIRNAME);
    bu_path_component(&fname, gedp->dbip->dbi_filename, BU_PATH_BASENAME);
    bu_vls_sprintf(&bkup_file, "%s/%d_gc_backup_%s", bu_vls_cstr(&fdir), bu_process_id(), bu_vls_cstr(&fname));
    if (bu_file_exists(bu_vls_cstr(&bkup_file), NULL)) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: %s is in the way, cannot proceed.\n", bu_vls_cstr(&bkup_file));
	bu_vls_printf(gedp->ged_result_str, "Aborting garbage collect, database unchanged.");
	ret = BRLCAD_ERROR;
	goto gc_cleanup;
    }

    // Stash processing data
    old_dbip = gedp->dbip;
    old_file_size = bu_file_size(gedp->dbip->dbi_filename);
    bu_vls_sprintf(&fpath, "%s", gedp->dbip->dbi_filename);

    /* For validation purposes, save the list of tops object names */
    db_update_nref(gedp->dbip, &rt_uniresource);
    path_cnt = db_ls(gedp->dbip, DB_LS_TOPS, NULL, &paths);
    for (int i = 0; i < path_cnt; i++) {
	old_top_objs.insert(std::string(paths[i]->d_namep));
    }
    bu_free(paths, "free db_ls output");
    paths = NULL;

    /* In addition to the database data itself, we also want to restore any
     * views to their original state when we open the garbage collected
     * database.  Save the who list. (TODO - do we need to save views?  Or
     * will drawing without resize work?) */
    ncmd = getenv("GED_TEST_NEW_CMD_FORMS");
    if (BU_STR_EQUAL(ncmd, "1")) {
	BViewState *bvs = gedp->dbi_state->get_view_state(gedp->ged_gvp);
	std::vector<std::string> wpaths = bvs->list_drawn_paths(-1, false);
	for (size_t i = 0; i < wpaths.size(); i++) {
	    who_objs.push_back(wpaths[i]);
	}
    } else {
	struct display_list *gdlp;
	for (BU_LIST_FOR(gdlp, display_list, gedp->ged_gdp->gd_headDisplay))
	    who_objs.push_back(std::string(bu_vls_cstr(&gdlp->dl_path)));
    }

    /* Create "working" database. */
    bu_vls_sprintf(&working_file, "%s/%d_gc_%s", bu_vls_cstr(&fdir),
	    bu_process_id(), bu_vls_cstr(&fname));
    bu_file_delete(bu_vls_cstr(&working_file));
    if (bu_file_exists(bu_vls_cstr(&working_file), NULL)) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: %s is in the way, cannot proceed.\n", bu_vls_cstr(&working_file));
	bu_vls_printf(gedp->ged_result_str, "Aborting garbage collect, database unchanged.");
	ret = BRLCAD_ERROR;
	goto gc_cleanup;
    }
    gc_wdbp = wdb_fopen_v(bu_vls_cstr(&working_file), db_version(old_dbip));
    if (gc_wdbp == NULL) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: unable to open working file %s, cannot proceed.\n", bu_vls_cstr(&working_file));
	bu_vls_printf(gedp->ged_result_str, "Aborting garbage collect, database unchanged.");
	bu_file_delete(bu_vls_cstr(&working_file));
	ret = BRLCAD_ERROR;
	goto gc_cleanup;
    }

    // First order of business, copy global properties not stored in database objects
    if (db_update_ident(gc_wdbp->dbip, old_dbip->dbi_title, old_dbip->dbi_local2base) < 0) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: db_update_ident() failed on %s, cannot proceed.\n", bu_vls_cstr(&working_file));
	bu_vls_printf(gedp->ged_result_str, "Aborting garbage collect, database unchanged.");
	db_close(gc_wdbp->dbip);
	bu_file_delete(bu_vls_cstr(&working_file));
	ret = BRLCAD_ERROR;
	goto gc_cleanup;
    }
    old_global_dp = db_lookup(gedp->dbip, DB5_GLOBAL_OBJECT_NAME, LOOKUP_QUIET);
    new_global_dp = db_lookup(gc_wdbp->dbip, DB5_GLOBAL_OBJECT_NAME, LOOKUP_QUIET);
    db5_get_attributes(old_dbip, &avs, old_global_dp);
    db5_update_attributes(new_global_dp, &avs, gc_wdbp->dbip);
    bu_avs_free(&avs);

    // Copy objects
    for (int i = 0; i < RT_DBNHASH; i++) {
	for (dp = old_dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    struct bu_external ext = BU_EXTERNAL_INIT_ZERO;
	    if (db_get_external(&ext, dp, old_dbip) < 0)
		continue;
	    int id = 0;
	    if (dp->d_flags & RT_DIR_COMB) {
		id = ID_COMBINATION;
	    } else {
		struct db5_raw_internal raw;
		if (db5_get_raw_internal_ptr(&raw, ext.ext_buf) != NULL) {
		    switch (raw.major_type) {
			case DB5_MAJORTYPE_BRLCAD:
			    id = raw.minor_type; break;
			case DB5_MAJORTYPE_BINARY_UNIF:
			    id = ID_BINUNIF; break;
			default:
			    id = 0;
		    }
		}
	    }
	    int flags = (dp->d_flags & RT_DIR_COMB) ? ((dp->d_flags & RT_DIR_REGION) ? RT_DIR_COMB | RT_DIR_REGION : RT_DIR_COMB) : RT_DIR_SOLID;
	    wdb_export_external(gc_wdbp, &ext, dp->d_namep, flags, id);
	}
    }
    db_close(gc_wdbp->dbip);


    // If we got this far, we need to close the current database, open the new
    // one, and verify the data
    av[0] = "closedb";
    av[1] = NULL;
    ged_exec(gedp, 1, (const char **)av);
    av[0] = "opendb";
    av[1] = bu_vls_cstr(&working_file);
    av[2] = NULL;
    if (ged_exec(gedp, 2, (const char **)av) != BRLCAD_OK) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: %s was not opened successfully!  Something is very wrong.  Aborting garbage collect, database unchanged\n", bu_vls_cstr(&working_file));
	ret = BRLCAD_ERROR;
	goto gc_cleanup;
    }

    // See what we've got for tops objects in the new file
    db_update_nref(gedp->dbip, &rt_uniresource);
    path_cnt = db_ls(gedp->dbip, DB_LS_TOPS, NULL, &paths);
    for (int i = 0; i < path_cnt; i++) {
	new_top_objs.insert(std::string(paths[i]->d_namep));
    }
    bu_free(paths, "free db_ls output");
    paths = NULL;

    // Validate the current tops set against the original
    std::set_difference(old_top_objs.begin(), old_top_objs.end(), new_top_objs.begin(), new_top_objs.end(), std::inserter(missing_old_top_objs, missing_old_top_objs.end()));
    std::set_difference(new_top_objs.begin(), new_top_objs.end(), old_top_objs.begin(), old_top_objs.end(), std::inserter(missing_new_top_objs, missing_new_top_objs.end()));
    if (missing_old_top_objs.size()) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: missing tops object(s):\n");
	for (s_it = missing_old_top_objs.begin(); s_it != missing_old_top_objs.end(); s_it++) {
	    bu_vls_printf(gedp->ged_result_str, "\t%s\n", (*s_it).c_str());
	}
	ret = BRLCAD_ERROR;
    }
    if (missing_new_top_objs.size()) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: new tops object(s):\n");
	for (s_it = missing_new_top_objs.begin(); s_it != missing_new_top_objs.end(); s_it++) {
	    bu_vls_printf(gedp->ged_result_str, "\t%s\n", (*s_it).c_str());
	}
	ret = BRLCAD_ERROR;
    }
    if (ret == BRLCAD_ERROR) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: %s has incorrect data!  Something is very wrong.  Aborting garbage collect, database unchanged\n", bu_vls_cstr(&working_file));
	av[0] = "closedb";
	av[1] = NULL;
	ged_exec(gedp, 1, (const char **)av);
	goto gc_cleanup;
    }

    // Done verifying
    av[0] = "closedb";
    av[1] = NULL;
    ged_exec(gedp, 1, (const char **)av);

    // Approaching the moment of truth, where we have to swap the new .g in for the
    // old one.  Time to back up the original file
    cfile.open(bu_vls_cstr(&fpath), std::ios::binary);
    ofile.open(bu_vls_cstr(&bkup_file), std::ios::binary);
    if (!cfile.is_open()) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: %s could not be backed up.\n", bu_vls_cstr(&fpath));
	bu_vls_printf(gedp->ged_result_str, "Aborting garbage collect, database unchanged.");
	ret = BRLCAD_ERROR;
	goto gc_cleanup;
    }
    if (!ofile.is_open()) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: %s could not be created.\n", bu_vls_cstr(&bkup_file));
	bu_vls_printf(gedp->ged_result_str, "Aborting garbage collect, database unchanged.");
	ret = BRLCAD_ERROR;
	cfile.close();
	goto gc_cleanup;
    }
    ofile << cfile.rdbuf();
    cfile.close();
    ofile.close();

    // *Gulp* - here we go.  Swap the new file in for the old
    bu_file_delete(bu_vls_cstr(&fpath));
    if (std::rename(bu_vls_cstr(&working_file), bu_vls_cstr(&fpath))) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: %s cannot be renamed to %s: %s!\nSomething is very wrong, aborting - user needs to manually restore %s to its original name/location.\n", bu_vls_cstr(&working_file), bu_vls_cstr(&fpath), strerror(errno), bu_vls_cstr(&bkup_file));
	ret = BRLCAD_ERROR;
	goto gc_cleanup;
    }

    // Open the renamed, garbage collected file
    av[0] = "opendb";
    av[1] = bu_vls_cstr(&fpath);
    av[2] = NULL;
    if (ged_exec(gedp, 2, (const char **)av) != BRLCAD_OK) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: %s was not opened successfully!\nSomething is very wrong, aborting - user needs to manually restore %s to its original name/location.\n", bu_vls_cstr(&fpath), bu_vls_cstr(&bkup_file));
	ret = BRLCAD_ERROR;
	goto gc_cleanup;
    }

    // Restore drawn object set
    av[0] = "draw";
    av[1] = "-R";
    for (size_t i = 0; i < who_objs.size(); i++) {
	av[2] = who_objs[i].c_str();
	ged_exec(gedp, 3, (const char **)av);
    }

    // We should be done now - check the sizes.  These warnings are not fatal
    // to the garbage collecting process, so they are done after the rename.
    new_file_size = bu_file_size(bu_vls_cstr(&fpath));
    if (new_file_size < old_file_size) {
	fs_percent = ((fastf_t)old_file_size - (fastf_t)new_file_size)/(fastf_t)old_file_size * 100;
	bu_vls_printf(gedp->ged_result_str, "Reduced by %d bytes (%g%% savings)\n", old_file_size - new_file_size, fs_percent);
	if (fs_percent > 50.0 && old_file_size > 512) {
	    bu_vls_printf(gedp->ged_result_str, "WARNING: Database size decreased substantially (more than 50%%)\n");
	    verify_failure++;
	}
    }
    if (new_file_size == old_file_size) {
	bu_vls_printf(gedp->ged_result_str, "Database size did NOT change.\n");
    }
    if (new_file_size > old_file_size) {
	fs_percent = ((fastf_t)new_file_size - (fastf_t)old_file_size)/(fastf_t)old_file_size * 100;
	bu_vls_printf(gedp->ged_result_str, "Increased by %d bytes (%g%% savings)\n", new_file_size - old_file_size, fs_percent);
	if (old_file_size > 512) {
	    bu_vls_printf(gedp->ged_result_str, "Database got bigger!  This should generally not happen.\n");
	    verify_failure++;
	}
    }

    if (verify_failure) {
	bu_vls_printf(gedp->ged_result_str, "Recommend verifying contents of %s by comparing with %s manually via gdiff. If a deeper, low level inspection is required the 'gex' tool is recommended.\n", bu_vls_cstr(&fpath), bu_vls_cstr(&bkup_file));
    }

    /* If everything succeeded, we're done and can remove the backup */
    if (!verify_failure) {
	bu_file_delete(bu_vls_cstr(&bkup_file));
    }

gc_cleanup:
    bu_vls_free(&bkup_file);
    bu_vls_free(&fdir);
    bu_vls_free(&fname);
    bu_vls_free(&fpath);
    bu_vls_free(&working_file);

    return ret;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
extern "C" {
    struct ged_cmd_impl garbage_collect_cmd_impl = { "garbage_collect", ged_garbage_collect_core, GED_CMD_DEFAULT };
    const struct ged_cmd garbage_collect_cmd = { &garbage_collect_cmd_impl };

    const struct ged_cmd *garbage_collect_cmds[] = { &garbage_collect_cmd,  NULL };

    static const struct ged_plugin pinfo = { GED_API,  garbage_collect_cmds, 1 };

    COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
    {
	return &pinfo;
    }
}
#endif

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

