/*                        U T I L . C P P
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
#include "wdb.h"
#include "../ged_private.h"
#include "./ged_facetize.h"

static const size_t FACETIZE_TERMINAL_DETAIL_LIMIT = 10u;
static const char FACETIZE_LOG_EXTENSION[] = ".log";
static const char FACETIZE_INSPECTION_LOG_SUFFIX[] = "-regions-to-inspect.log";
static const char FACETIZE_TOLERATED_FAILURE_LOG_SUFFIX[] = "-tolerated-failures.log";

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

void
facetize_failure_clear(struct _ged_facetize_state *s)
{
    if (!s || !s->failure_msg)
	return;

    bu_vls_trunc(s->failure_msg, 0);
}

void
facetize_failure(struct _ged_facetize_state *s, const char *fmt, ...)
{
    va_list ap;

    if (!s || !s->failure_msg)
	return;
    if (UNLIKELY(!fmt || strlen(fmt) == 0))
	return;

    /* Preserve the first concrete failure cause.  Later cleanup/write errors
     * are usually consequences, and replacing the first cause makes reports
     * less actionable. */
    if (bu_vls_strlen(s->failure_msg))
	return;

    va_start(ap, fmt);
    bu_vls_vprintf(s->failure_msg, fmt, ap);
    va_end(ap);
}

void
facetize_tolerated_failure(struct _ged_facetize_state *s, const char *fmt, ...)
{
    va_list ap;
    struct bu_vls detail = BU_VLS_INIT_ZERO;

    if (!s || !s->tolerate_failures)
	return;
    if (UNLIKELY(!fmt || strlen(fmt) == 0))
	return;

    s->tolerated_failures++;

    va_start(ap, fmt);
    bu_vls_vprintf(&detail, fmt, ap);
    va_end(ap);

    if (s->lfile) {
	fprintf(s->lfile, "TOLERATED FAILURE: %s\n", bu_vls_cstr(&detail));
	fflush(s->lfile);
    }

    if (s->tolerated_failure_log) {
	bu_vls_printf(s->tolerated_failure_log, "      - %s\n", bu_vls_cstr(&detail));
	s->tolerated_failure_details++;
    } else {
	s->tolerated_failure_omitted++;
    }

    bu_vls_free(&detail);
}

static size_t
facetize_detail_line_count(const struct bu_vls *details)
{
    if (!details || !bu_vls_strlen(details))
	return 0;

    const char *detail_text = bu_vls_cstr(details);
    size_t detail_length = bu_vls_strlen(details);
    size_t line_count = 0;
    for (size_t i = 0; i < detail_length; i++) {
	if (detail_text[i] == '\n')
	    line_count++;
    }
    if (detail_text[detail_length - 1] != '\n')
	line_count++;
    return line_count;
}

static void
facetize_detail_log_path(struct bu_vls *path,
	const struct _ged_facetize_state *s, const char *suffix)
{
    if (!path || !s || !suffix)
	return;

    struct bu_vls log_basename = BU_VLS_INIT_ZERO;
    const char *main_log = "facetize";
    if (s->log_file && bu_vls_strlen(s->log_file)) {
	main_log = bu_vls_cstr(s->log_file);
	if (s->log_file_is_temporary &&
		bu_path_component(&log_basename, main_log, BU_PATH_BASENAME))
	    main_log = bu_vls_cstr(&log_basename);
    }
    bu_vls_sprintf(path, "%s", main_log);
    size_t path_length = bu_vls_strlen(path);
    const size_t extension_length = sizeof(FACETIZE_LOG_EXTENSION) - 1;
    if (path_length >= extension_length &&
	    BU_STR_EQUAL(bu_vls_cstr(path) + path_length - extension_length,
		    FACETIZE_LOG_EXTENSION))
	bu_vls_trunc(path, -(int)extension_length);
    bu_vls_strcat(path, suffix);
    bu_vls_free(&log_basename);
}

static bool
facetize_write_detail_log(const struct bu_vls *path, const char *title,
	size_t detail_count, const struct bu_vls *details)
{
    if (!path || !bu_vls_strlen(path) || !title || !details)
	return false;

    std::ofstream detail_file(bu_vls_cstr(path),
	    std::ios::out | std::ios::trunc);
    if (!detail_file)
	return false;

    detail_file << title << " (" << detail_count << ")\n\n"
	<< bu_vls_cstr(details);
    detail_file.close();
    return detail_file.good();
}

static void
facetize_report_details(struct _ged_facetize_state *s, const char *label,
	size_t detail_count, const struct bu_vls *details, const char *log_suffix)
{
    if (!s || !label || !detail_count || !details ||
	    !bu_vls_strlen(details) || !log_suffix)
	return;

    if (facetize_detail_line_count(details) <= FACETIZE_TERMINAL_DETAIL_LIMIT) {
	facetize_log(s, 0, "\n    %s:\n%s", label, bu_vls_cstr(details));
	return;
    }

    struct bu_vls detail_path = BU_VLS_INIT_ZERO;
    facetize_detail_log_path(&detail_path, s, log_suffix);
    if (facetize_write_detail_log(&detail_path, label, detail_count, details)) {
	facetize_log(s, 0, "\n    %s:\n", label);
	facetize_log(s, 0, "      Complete list written to %s\n",
		bu_vls_cstr(&detail_path));
    } else {
	/* Preserve the details if the dedicated file cannot be created. */
	facetize_log(s, 0, "\n    %s:\n", label);
	facetize_log(s, 0,
		"      Unable to write %s; complete list follows:\n%s",
		bu_vls_cstr(&detail_path), bu_vls_cstr(details));
    }
    bu_vls_free(&detail_path);
}

void
facetize_summary(struct _ged_facetize_state *s)
{
    if (!s)
	return;

    bool have_region_summary = s->region_summary &&
	bu_vls_strlen(s->region_summary);
    bool have_primitive_summary = s->primitive_summary &&
	bu_vls_strlen(s->primitive_summary);
    bool have_tolerated_failures = s->tolerate_failures &&
	s->tolerated_failures > 0;
    if (!have_region_summary && !have_primitive_summary &&
	    !have_tolerated_failures)
	return;

    facetize_log(s, 0, "\nFACETIZE summary:\n");
    if (have_region_summary)
	facetize_log(s, 0, "%s", bu_vls_cstr(s->region_summary));
    if (s->inspection_regions > 0)
	facetize_report_details(s, "Regions to inspect manually",
		s->inspection_regions, s->inspection_log,
		FACETIZE_INSPECTION_LOG_SUFFIX);

    if (have_primitive_summary)
	facetize_log(s, 0, "%s", bu_vls_cstr(s->primitive_summary));

    if (have_tolerated_failures) {
	facetize_log(s, 0, "\n  Tolerated failures:\n");
	facetize_log(s, 0, "    %-43s %8d\n", "Components omitted",
		s->tolerated_failures);
	facetize_log(s, 0,
		"    WARNING: output is partial and does not completely represent the input.\n");
	facetize_log(s, 0,
		"    Re-run without --tolerate-failures to stop at the first failure.\n");
	if (s->tolerated_failure_details > 0)
	    facetize_report_details(s, "Tolerated failure details",
		    (size_t)s->tolerated_failure_details,
		    s->tolerated_failure_log,
		    FACETIZE_TOLERATED_FAILURE_LOG_SUFFIX);
	if (s->tolerated_failure_omitted > 0)
	    facetize_log(s, 0,
		    "    %d additional failure detail(s) unavailable; see %s for context.\n",
		    s->tolerated_failure_omitted,
		    (s->log_file && bu_vls_strlen(s->log_file)) ?
		    bu_vls_cstr(s->log_file) : "the facetize log");
    }
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

    /* In-memory databases do not have the file allocator used by
     * rt_db_put_internal.  Route them through their dedicated writer so
     * isolated workers can finish evaluation before announcing a staged
     * disk write to the parent. */
    if (!dbip->dbi_filename) {
	struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
	if (!wdbp || wdb_put_internal(wdbp, name, &intern, 1.0) < 0) {
	    if (verbosity >= 0)
		bu_log("Failed to write %s to in-memory database\n", name);
	    rt_db_free_internal(&intern);
	    return BRLCAD_ERROR;
	}
	return BRLCAD_OK;
    }

    struct directory *dp = db_diradd(dbip, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	if (verbosity >= 0)
	    bu_log("Cannot add %s to directory\n", name);
	return BRLCAD_ERROR;
    }

    if (rt_db_put_internal(dp, dbip, &intern) < 0) {
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

    struct db_i *dbip = s->dbip;
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
		    if (rt_db_get_internal(&intern, ldp, dbip, NULL) < 0)
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
    FOR_ALL_DIRECTORY_START(dp, dbip)
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
    FOR_ALL_DIRECTORY_END;

    return BRLCAD_OK;
}

struct rt_bot_internal *
bot_fixup(struct _ged_facetize_state *s, struct db_i *wdbip, struct directory *bot_dp, const char *bname)
{

    facetize_log(s, 2, "\t%s: checking Boolean result for thin faces...\n", bname);

    // Unpack the existing bot
    if (!bot_dp)
	return NULL;

    struct rt_db_internal bot_intern;
    RT_DB_INTERNAL_INIT(&bot_intern);
    if (rt_db_get_internal(&bot_intern, bot_dp, wdbip, NULL) < 0) {
	return NULL;
    }

    struct rt_bot_internal *bot = (struct rt_bot_internal *)(bot_intern.idb_ptr);
    if (!bot->num_faces) {
	rt_db_free_internal(&bot_intern);
	return NULL;
    }

    // Have faces, test with raytracer
    struct rt_i *rtip = rt_i_create(wdbip);
    rt_gettree(rtip, bname);
    rt_prep(rtip);
    facetize_log(s, 2, "\t%s: raytrace preparation complete; scanning %zu faces...\n", bname, bot->num_faces);
    struct bu_ptbl tfaces = BU_PTBL_INIT_ZERO;
    int have_thin_faces = rt_bot_thin_check(&tfaces, bot, rtip, VUNITIZE_TOL, 0);
    rt_i_destroy(rtip);
    facetize_log(s, 2, "\t%s: thin-face scan complete; %zu face(s) flagged.\n", bname, BU_PTBL_LEN(&tfaces));

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
    facetize_log(s, 2, "\t%s: removed %zu thin face(s); attempting manifold repair...\n", bname, removed_face_cnt);

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
	rt_bot_internal_free(nbot);
	BU_PUT(nbot, struct rt_bot_internal);
	bu_ptbl_free(&tfaces);
	facetize_log(s, 2, "\t%s: all %zd faces flagged as thin; retaining original manifold result.\n", bname, removed_face_cnt);
	return NULL;
    }

    // If we took away manifoldness removing faces (very likely) we need to try
    // and rebuild it.
    struct rt_bot_internal *rbot = NULL;
    struct rt_bot_repair_info rs = RT_BOT_REPAIR_INFO_INIT;
    rs.strict = 0;
    int repair_result = rt_bot_repair(&rbot, nbot, &rs);
    facetize_log(s, 2, "\t%s: conservative manifold repair returned %d.\n", bname, repair_result);
    if (repair_result < 0) {
	// If a conservative repair fails, try being a little
	// more aggressive
	rs.max_hole_area_percent = 30;
	repair_result = rt_bot_repair(&rbot, nbot, &rs);
	facetize_log(s, 2, "\t%s: aggressive manifold repair returned %d.\n", bname, repair_result);
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

    struct rt_i *crtip = rt_i_create(wdbip);
    rt_gettree(crtip, test_name);
    rt_prep(crtip);
    bu_ptbl_reset(&tfaces);
    have_thin_faces = rt_bot_thin_check(&tfaces, rbot, crtip, VUNITIZE_TOL, 0);
    rt_i_destroy(crtip);
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
    facetize_log(s, 1, "\t%s removed %zd thin faces, new manifold BoT created.\n", bname, removed_face_cnt);
    bu_ptbl_free(&tfaces);
    return rbot;
}

void
facetize_collect_primitive_summary(struct _ged_facetize_state *s)
{
    if (!s || !s->primitive_summary)
	return;

    struct db_i *dbip = s->dbip;
    bu_vls_trunc(s->primitive_summary, 0);

    std::map<std::string, std::set<std::string>> method_sets;
    std::map<std::string, std::set<std::string>>::iterator m_it;
    std::set<std::string>::iterator s_it;
    struct db_i *cdbip = db_open(bu_vls_cstr(s->wfile), DB_OPEN_READONLY);
    if (!cdbip)
	return;

    bu_vls_printf(s->primitive_summary, "\n  Primitive tessellation:\n");
    db_dirbuild(cdbip);
    db_update_nref(cdbip);
    method_scan(&method_sets, cdbip);
    size_t total = 0;
    size_t fail_cnt = 0;
    size_t repair_cnt = 0;
    size_t plate_cnt = 0;
    for (m_it = method_sets.begin(); m_it != method_sets.end(); ++m_it) {
	total += m_it->second.size();
	if (m_it->first == std::string("FAIL")) fail_cnt += m_it->second.size();
	if (m_it->first == std::string("REPAIR")) repair_cnt += m_it->second.size();
	if (m_it->first == std::string("PLATE")) plate_cnt += m_it->second.size();
    }
    bu_vls_printf(s->primitive_summary, "    %-43s %8zu\n", "Total solids evaluated", total);
    bu_vls_printf(s->primitive_summary, "    %-43s %8zu\n", "Failed tessellation", fail_cnt);
    bu_vls_printf(s->primitive_summary, "    %-43s %8zu\n", "Plate extrusions", plate_cnt);
    bu_vls_printf(s->primitive_summary, "    %-43s %8zu\n", "BoT repair closures", repair_cnt);
    bu_vls_printf(s->primitive_summary, "\n    Method breakdown:\n");
    for (m_it = method_sets.begin(); m_it != method_sets.end(); ++m_it) {
	if (m_it->first == std::string("REPAIR")) {
	    bu_vls_printf(s->primitive_summary, "      %-41s %8zu\n", "bot repair", m_it->second.size());
	} else if (m_it->first == std::string("PLATE")) {
	    bu_vls_printf(s->primitive_summary, "      %-41s %8zu\n", "plate extrusion", m_it->second.size());
	} else if (m_it->first == std::string("FAIL")) {
	    bu_vls_printf(s->primitive_summary, "      %-41s %8zu\n", "failed", m_it->second.size());
	} else {
	    std::string mlabel = std::string("success: ") + m_it->first;
	    bu_vls_printf(s->primitive_summary, "      %-41s %8zu\n", mlabel.c_str(), m_it->second.size());
	}
	if (s->verbosity > 1) {
	    // If we used NMG to facetize, that's considered normal - don't
	    // bother listing those primitives
	    if (m_it->first == std::string("NMG"))
		continue;
	    for (s_it = m_it->second.begin(); s_it != m_it->second.end(); ++s_it) {
		bu_vls_printf(s->primitive_summary, "        %s\n",
			s_it->c_str());
	    }
	}
    }
    db_close(cdbip);

    // Make combs with the various categories of object that weren't a
    // standard successful NMG ft_tessellate run
    struct bu_vls cname = BU_VLS_INIT_ZERO;
    for (m_it = method_sets.begin(); m_it != method_sets.end(); ++m_it) {
	if (m_it->first == std::string("NMG"))
	    continue;
	bu_vls_sprintf(&cname, "facetize_%s_objs", m_it->first.c_str());
	if (db_lookup(dbip, bu_vls_cstr(&cname), LOOKUP_QUIET) != RT_DIR_NULL)
	    bu_vls_incr(&cname, NULL, NULL, &_db_uniq_test, (void *)dbip);
	struct wmember wcomb;
	BU_LIST_INIT(&wcomb.l);
	struct rt_wdb *cwdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DEFAULT);
	for (s_it = m_it->second.begin(); s_it != m_it->second.end(); ++s_it) {
	    (void)mk_addmember(s_it->c_str(), &(wcomb.l), NULL, DB_OP_UNION);
	}
	mk_lcomb(cwdbp, bu_vls_cstr(&cname), &wcomb, 0, NULL, NULL, NULL, 0);
    }
    bu_vls_free(&cname);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
