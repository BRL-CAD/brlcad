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
#include "rt/primitives/bot.h"
#include "../ged_private.h"
#include "./ged_facetize.h"

void
facetize_log(struct _ged_facetize_state *s, int msg_level, const char *fmt, ...)
{
    va_list ap;
    struct bu_vls output = BU_VLS_INIT_ZERO;

    if (!s)
	return;
    if (UNLIKELY(!fmt || strlen(fmt) == 0))
	return;

    va_start(ap, fmt);
    bu_vls_vprintf(&output, fmt, ap);
    va_end(ap);

    if (s->lfile) {
	fprintf(s->lfile, "%s", bu_vls_cstr(&output));
	fflush(s->lfile);
    }

    // If verbosity level is high enough, also print immediately
    if (s->verbosity >= msg_level)
	bu_log("%s", bu_vls_cstr(&output));

    bu_vls_free(&output);
}

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

    bu_avs_init_empty(&intern.idb_avs);
    (void)bu_avs_add(&intern.idb_avs, "facetized", "1");

    struct directory *dp = db_diradd(dbip, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	if (verbosity >= 0)
	    bu_log("Cannot add %s to directory\n", name);
	return BRLCAD_ERROR;
    }

    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	if (verbosity >= 0)
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
_ged_facetize_working_file_setup(struct _ged_facetize_state *s, struct bu_ptbl *leaf_dps)
{
    if (!s)
	return BRLCAD_ERROR;

    struct db_i *dbip = s->gedp->dbip;
    int resume = s->resume;

    if (!bu_vls_strlen(s->wfile)) {
	char tmpwfile[MAXPATHLEN];
	struct bu_vls wfilename = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&wfilename, "facetize_%s", bu_vls_cstr(s->bname));
	bu_dir(tmpwfile, MAXPATHLEN, s->wdir, bu_vls_cstr(&wfilename), NULL);
	bu_vls_sprintf(s->wfile, "%s",tmpwfile);
	bu_vls_free(&wfilename);
    }

    // If we have a wfile, check for a pid and see if that pid is still active.  If it is, bail - we
    // can't collide with another active process trying to facetize the same .g file
    int pid;
    if (bu_file_exists(bu_vls_cstr(s->wfile), NULL)) {
	struct db_i *wdbip = db_open(bu_vls_cstr(s->wfile), DB_OPEN_READONLY);
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
		    if (bu_pid_alive(pid)) {
			bu_log("Error - %s _GLOBAL attribute process id %d is still active.  This indicates another process is actively working on this file.  Please terminate process %d before trying a facetize operation on this .g file.", bu_vls_cstr(s->wfile), pid, pid);
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

    // If we're resuming, the resuming process is taking ownership of the
    // existing working file
    int write_pid = resume;

    if (!bu_file_exists(bu_vls_cstr(s->wfile), NULL)) {
	// Populate the working copy with original .g data
	// (TODO - should use the dbip's FILE pointer for this rather than
	// opening it again if we can (see FIO24-C), but that's a private entry
	// in db_i per the header.  Maybe need a function to return a FILE *
	// from a struct db_i?)
	std::ifstream orig_file(dbip->dbi_filename, std::ios::binary);
	std::ofstream work_file(bu_vls_cstr(s->wfile), std::ios::binary);
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
		    dsp_data_cpy(dbip, (struct rt_dsp_internal *)intern.idb_ptr, s->wdir);
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
	pid = bu_pid();
	struct db_i *wdbip = db_open(bu_vls_cstr(s->wfile), DB_OPEN_READWRITE);
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

    if (!bu_file_exists(bu_vls_cstr(s->wfile), NULL))
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}

int
method_scan(std::map<std::string, std::set<std::string>> *method_sets, struct db_i *dbip)
{
    if (!method_sets || !dbip)
	return BRLCAD_ERROR;

    struct directory *dp;
    for (int i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {
	    if (dp->d_minor_type != DB5_MINORTYPE_BRLCAD_BOT)
		continue;
	    struct bu_attribute_value_set avs = BU_AVS_INIT_ZERO;
	    if (db5_get_attributes(dbip, &avs, dp))
		continue;
	    const char *method = bu_avs_get(&avs, FACETIZE_METHOD_ATTR);
	    if (!method || !strlen(method)) {
		bu_avs_free(&avs);
		continue;
	    }
	    (*method_sets)[std::string(method)].insert(std::string(dp->d_namep));
	    bu_avs_free(&avs);
	}
    }

    return BRLCAD_OK;
}

struct rt_bot_internal *
bot_fixup(struct _ged_facetize_state *s, struct db_i *wdbip, struct directory *bot_dp, const char *bname)
{

    // Unpack the existing bot
    if (!bot_dp)
	return NULL;

    struct rt_db_internal bot_intern;
    RT_DB_INTERNAL_INIT(&bot_intern);
    if (rt_db_get_internal(&bot_intern, bot_dp, wdbip, NULL, &rt_uniresource) < 0) {
	return NULL;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(bot_intern.idb_ptr);
    if (!bot->num_faces) {
	rt_db_free_internal(&bot_intern);
	return NULL;
    }

    // Have faces, test with raytracer
    struct rt_i *rtip = rt_new_rti(wdbip);
    rt_gettree(rtip, bname);
    rt_prep(rtip);
    struct bu_ptbl tfaces = BU_PTBL_INIT_ZERO;
    int have_thin_faces = rt_bot_thin_check(&tfaces, bot, rtip, VUNITIZE_TOL, 0);
    rt_free_rti(rtip);

    // No problematic faces reported, nothing to do
    if (!have_thin_faces) {
	rt_db_free_internal(&bot_intern);
	bu_ptbl_free(&tfaces);
	return NULL;
    }

    // If we do have a problem, first order of business - get the problematic faces out of
    // the mesh
    struct rt_bot_internal *nbot = rt_bot_remove_faces(&tfaces, bot);
    size_t removed_face_cnt = BU_PTBL_LEN(&tfaces);

    // Done with original bot
    rt_db_free_internal(&bot_intern);

    // If we didn't get a new bot, we're done
    if (!nbot) {
	bu_ptbl_free(&tfaces);
	return NULL;
    }

    // Return an empty bot, if that's what was created (can happen legitimately
    // when all the faces are thin - i.e. a degenerate volume.)
    if (!nbot->num_faces) {
	bu_ptbl_free(&tfaces);
	return nbot;
    }

    // If we took away manifoldness removing faces (very likely) we need to try
    // and rebuild it.
    struct rt_bot_internal *rbot = NULL;
    struct rt_bot_repair_info rs = RT_BOT_REPAIR_INFO_INIT;
    int repair_result = rt_bot_repair(&rbot, nbot, &rs);
    if (repair_result < 0) {
	// If a conservative repair fails, try being a little
	// more aggressive
	rs.max_hole_area_percent = 30;
	repair_result = rt_bot_repair(&rbot, nbot, &rs);
    }

    if (repair_result < 0 || !rbot || !rbot->num_faces) {
	facetize_log(s, 2, "\t%s attempted to remove %zd thin faces, but unable to produce a new manifold BoT.  Retaining original manifold result.\n", bname, removed_face_cnt);
	// The repair didn't succeed.  That means we weren't able to produce a
	// manifold mesh after removing the thin triangles.  In that situation,
	// we return the manifold result we do have, thin triangles or not,
	// rather than produce something invalid.
	if (rbot && rbot != nbot) {
	    rt_bot_internal_free(rbot);
	    BU_PUT(rbot, struct rt_bot_internal);
	}
	rt_bot_internal_free(nbot);
	BU_PUT(nbot, struct rt_bot_internal);
	bu_ptbl_free(&tfaces);
	return NULL;
    }

    // If repair didn't have to change anything (very unlikely) go with the result
    if (UNLIKELY(rbot == nbot)) {
	bu_ptbl_free(&tfaces);
	return nbot;
    }

    // We have a repaired mesh, and it's different from nbot - done with nbot
    rt_bot_internal_free(nbot);
    BU_PUT(nbot, struct rt_bot_internal);

    // Nominally successful - check to see if the new output is free of the
    // problems seen in the original.  If so, use it - if not, stick with the
    // original.
    const char *test_name = "__facetize_repair_check.bot__";
    // Delete a conflicting test object name, if present
    struct directory *odp = db_lookup(wdbip, test_name, LOOKUP_QUIET);
    if (odp) {
	db_delete(wdbip, odp);
	db_dirdelete(wdbip, odp);
    }

    // Writing the bot is destructive to the rbot container, so we need a copy.
    struct rt_bot_internal *test_bot = rt_bot_dup(rbot);
    if (_ged_facetize_write_bot(wdbip, test_bot, test_name, 0)) {
	// Couldn't test - we're done
	rt_bot_internal_free(rbot);
	BU_PUT(rbot, struct rt_bot_internal);
	bu_ptbl_free(&tfaces);
	return NULL;
    }

    struct rt_i *crtip = rt_new_rti(wdbip);
    rt_gettree(crtip, test_name);
    rt_prep(crtip);
    bu_ptbl_reset(&tfaces);
    have_thin_faces = rt_bot_thin_check(&tfaces, rbot, crtip, VUNITIZE_TOL, 0);
    rt_free_rti(crtip);
    // Win or lose, delete the test obj
    odp = db_lookup(wdbip, test_name, LOOKUP_QUIET);
    if (odp) {
	db_delete(wdbip, odp);
	db_dirdelete(wdbip, odp);
    }

    if (have_thin_faces) {
	// Still not clean - we're done
	facetize_log(s, 2, "\t%s removed %zd thin faces, but new manifold did not pass rt_bot_thin_check - %zd thin faces were found after repair attempt.  Retaining original manifold result.\n", bname, removed_face_cnt, BU_PTBL_LEN(&tfaces));
	rt_bot_internal_free(rbot);
	bu_ptbl_free(&tfaces);
	BU_PUT(rbot, struct rt_bot_internal);
	return NULL;
    }

    // Successfully produced a new, clean mesh
    facetize_log(s, 0, "\t%s removed %zd thin faces, new manifold BoT created.\n", bname, removed_face_cnt);
    bu_ptbl_free(&tfaces);
    return rbot;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

