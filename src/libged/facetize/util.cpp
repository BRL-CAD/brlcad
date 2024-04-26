/*                        U T I L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2024 United States Government as represented by
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
/** @file libged/facetize/util.cpp
 *
 * facetize command.
 *
 */

#include "common.h"

#include <string.h>

#include <iostream>
#include <fstream>

#include "bu/app.h"
#include "bu/path.h"
#include "bu/ptbl.h"
#include "rt/search.h"
#include "rt/db_instance.h"
#include "../ged_private.h"
#include "./ged_facetize.h"

int
_db_uniq_test(struct bu_vls *n, void *data)
{
    struct db_i *dbip = (struct db_i *)data;
    if (db_lookup(dbip, bu_vls_addr(n), LOOKUP_QUIET) == RT_DIR_NULL) return 1;
    return 0;
}

int
_ged_validate_objs_list(struct _ged_facetize_state *s, int argc, const char *argv[], int newobj_cnt)
{
    int i;
    struct ged *gedp = s->gedp;

    if (s->in_place && newobj_cnt) {
	bu_vls_printf(gedp->ged_result_str, "In place conversion specified, but object list includes objects that do not exist:\n");
	for (i = argc - newobj_cnt; i < argc; i++) {
	    bu_vls_printf(gedp->ged_result_str, "       %s\n", argv[i]);
	}
	bu_vls_printf(gedp->ged_result_str, "\nAborting.  When performing an in-place facetization, a single pre-existing object must be specified.\n");
	return BRLCAD_ERROR;

    }

    if (!s->in_place) {
	if (newobj_cnt < 1) {
	    bu_vls_printf(gedp->ged_result_str, "all objects listed already exist, aborting.  (Need new object name to write out results to.)\n");
	    return BRLCAD_ERROR;
	}

	if (newobj_cnt > 1) {
	    bu_vls_printf(gedp->ged_result_str, "More than one object listed does not exist:\n");
	    for (i = argc - newobj_cnt; i < argc; i++) {
		bu_vls_printf(gedp->ged_result_str, "   %s\n", argv[i]);
	    }
	    bu_vls_printf(gedp->ged_result_str, "\nAborting.  Need to specify exactly one object name that does not exist to hold facetization output.\n");
	    return BRLCAD_ERROR;
	}

	if (argc - newobj_cnt == 0) {
	    bu_vls_printf(gedp->ged_result_str, "No existing objects specified, nothing to facetize.  Aborting.\n");
	    return BRLCAD_ERROR;
	}
    }

    return BRLCAD_OK;
}

int
_ged_facetize_write_bot(struct db_i *dbip, struct rt_bot_internal *bot, const char *name, int verbosity)
{
    /* Export BOT as a new solid */
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_BOT;
    intern.idb_meth = &OBJ[ID_BOT];
    intern.idb_ptr = (void *)bot;

    struct directory *dp = db_diradd(dbip, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	if (verbosity)
	    bu_log("Cannot add %s to directory\n", name);
	return BRLCAD_ERROR;
    }

    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	if (verbosity)
	    bu_log("Failed to write %s to database\n", name);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

static void
dsp_data_cpy(struct db_i *dbip, struct rt_dsp_internal *dsp_ip, const char *dirpath)
{
    if (!dbip || !dsp_ip || !dirpath)
	return;

    // Need to look for any local referenced data files and also make copies of
    // those into wdir.  An example is the terra.bin file from the example dsp
    // - that is a local file reference, so the cache copy needs its own local
    // version of the referenced file in the proper relative position.
    if (dsp_ip->dsp_datasrc == RT_DSP_SRC_V4_FILE || dsp_ip->dsp_datasrc == RT_DSP_SRC_FILE) {
	char * const *pathp = dbip->dbi_filepath;
	struct bu_vls dpath = BU_VLS_INIT_ZERO;
	for (; *pathp != NULL; pathp++) {
	    bu_vls_strcpy(&dpath , *pathp);
	    bu_vls_putc(&dpath, '/');
	    bu_vls_strcat(&dpath, bu_vls_cstr(&dsp_ip->dsp_name));
	    if (bu_file_exists(bu_vls_cstr(&dpath), NULL))
		break;
	}
	if (!bu_vls_strlen(&dpath))
	    return;
	char wpath[MAXPATHLEN];
	bu_dir(wpath, MAXPATHLEN, dirpath, bu_vls_cstr(&dsp_ip->dsp_name), NULL);
	std::ifstream orig_file(bu_vls_cstr(&dpath), std::ios::binary);
	std::ofstream work_file(wpath, std::ios::binary);
	if (!orig_file.is_open() || !work_file.is_open()) {
	    bu_vls_free(&dpath);
	    return;
	}
	work_file << orig_file.rdbuf();
	orig_file.close();
	work_file.close();
	bu_vls_free(&dpath);
    }
}

#define PID_KEY "facetize_pid"
int
_ged_facetize_working_file_setup(char **wfile, char **wdir, struct db_i *dbip, struct bu_ptbl *leaf_dps, int resume)
{
    if (!dbip || !wfile || !wdir)
	return BRLCAD_ERROR;


    if (!*wfile || !*wdir) {
	(*wfile) = (char *)bu_calloc(MAXPATHLEN, sizeof(char), "wfile");
	(*wdir) = (char *)bu_calloc(MAXPATHLEN, sizeof(char), "wdir");
	/* Figure out the working .g filename */
	struct bu_vls wfilename = BU_VLS_INIT_ZERO;
	struct bu_vls bname = BU_VLS_INIT_ZERO;
	struct bu_vls dname = BU_VLS_INIT_ZERO;
	char rfname[MAXPATHLEN];
	bu_file_realpath(dbip->dbi_filename, rfname);

	// Get the root filename, so we can give the working file a relatable name
	bu_path_component(&bname, rfname, BU_PATH_BASENAME);

	// Hash the path string and construct
	unsigned long long hash_num = bu_data_hash((void *)bu_vls_cstr(&bname), bu_vls_strlen(&bname));
	bu_vls_sprintf(&dname, "facetize_%llu", hash_num);
	bu_vls_sprintf(&wfilename, "facetize_%s", bu_vls_cstr(&bname));
	bu_vls_free(&bname);

	// Have filename, get a location in the cache directory
	bu_dir(*wdir, MAXPATHLEN, BU_DIR_CACHE, bu_vls_cstr(&dname), NULL);
	bu_dir(*wfile, MAXPATHLEN, BU_DIR_CACHE, bu_vls_cstr(&dname), bu_vls_cstr(&wfilename), NULL);
	bu_vls_free(&dname);
	bu_vls_free(&wfilename);
    }

    // If we have a wfile, check for a pid and see if that pid is still active.  If it is, bail - we
    // can't collide with another active process trying to facetize the same .g file
    int pid;
    if (bu_file_exists(*wfile, NULL)) {
	struct db_i *wdbip = db_open(*wfile, DB_OPEN_READONLY);
	if (wdbip) {
	    if (db_dirbuild(wdbip) < 0)
		return BRLCAD_ERROR;
	    struct bu_attribute_value_set avs;
	    bu_avs_init_empty(&avs);
	    struct directory *wgdp = db_lookup(wdbip, DB5_GLOBAL_OBJECT_NAME, LOOKUP_QUIET);
	    if (!wgdp)
		return BRLCAD_ERROR;
	    if (db5_get_attributes(wdbip, &avs, wgdp)) {
		const char *val = bu_avs_get(&avs, PID_KEY);
		if (bu_opt_int(NULL, 1, (const char **)&val, (void *)&pid) == 1) {
		    if (bu_process_alive_id(pid)) {
			bu_log("Error - %s _GLOBAL attribute process id %d is still active.  This indicates another process is actively working on this file.  Please terminate process %d before trying a facetize operation on this .g file.", *wfile, pid, pid);
			bu_avs_free(&avs);
			db_close(wdbip);
			return BRLCAD_ERROR;
		    }
		}
	    }
	    bu_avs_free(&avs);
	    db_close(wdbip);
	}
    }

    // If we're starting over, clear the old working directory
    if (!resume && bu_file_directory(*wdir)) {
	bu_dirclear(*wdir);
    }

    if (!bu_file_directory(*wdir)) {
	// Set up the directory
	bu_mkdir(*wdir);
    }

    // If we're resuming, the resuming process is taking ownership of the
    // existing working file
    int write_pid = resume;

    if (!bu_file_exists(*wfile, NULL)) {
	// Populate the working copy with original .g data
	// (TODO - should use the dbip's FILE pointer for this rather than
	// opening it again if we can (see FIO24-C), but that's a private entry
	// in db_i per the header.  Maybe need a function to return a FILE *
	// from a struct db_i?)
	std::ifstream orig_file(dbip->dbi_filename, std::ios::binary);
	std::ofstream work_file(*wfile, std::ios::binary);
	if (!orig_file.is_open() || !work_file.is_open())
	    return BRLCAD_ERROR;
	work_file << orig_file.rdbuf();
	orig_file.close();
	work_file.close();

	// Must also copy any files referenced by the .g into the proper
	// relative position to the working .g copy.
	if (leaf_dps) {
	    for (size_t i = 0; i < BU_PTBL_LEN(leaf_dps); i++) {
		struct directory *ldp = (struct directory *)BU_PTBL_GET(leaf_dps, i);
		if (ldp->d_major_type != DB5_MAJORTYPE_BRLCAD)
		    continue;

		if (ldp->d_minor_type == ID_DSP) {
		    struct rt_db_internal intern;
		    if (rt_db_get_internal(&intern, ldp, dbip, NULL, &rt_uniresource) < 0)
			continue;
		    dsp_data_cpy(dbip, (struct rt_dsp_internal *)intern.idb_ptr, *wdir);
		    rt_db_free_internal(&intern);
		}

		// TODO - There may be other such cases...
	    }
	}

	// We have created a new file, so we need to set the pid
	write_pid = 1;
    }

    if (write_pid) {
	// Write the current pid to the working file as a _GLOBAL attribute
	pid = bu_process_id();
	struct db_i *wdbip = db_open(*wfile, DB_OPEN_READWRITE);
	if (wdbip) {
	    if (db_dirbuild(wdbip) < 0)
		return BRLCAD_ERROR;
	    struct bu_attribute_value_set avs;
	    bu_avs_init_empty(&avs);
	    struct directory *wgdp = db_lookup(wdbip, DB5_GLOBAL_OBJECT_NAME, LOOKUP_QUIET);
	    if (!wgdp)
		return BRLCAD_ERROR;
	    if (db5_get_attributes(wdbip, &avs, wgdp)) {
		struct bu_vls pid_str = BU_VLS_INIT_ZERO;
		bu_vls_sprintf(&pid_str, "%d", pid);
		(void)bu_avs_add(&avs, PID_KEY, bu_vls_cstr(&pid_str));
		(void)db5_update_attributes(wgdp, &avs, wdbip);
		bu_avs_free(&avs);
	    }
	    db_close(wdbip);
	}
    }

    if (!bu_file_exists(*wfile, NULL))
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

