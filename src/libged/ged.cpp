/*                       G E D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2000-2026 United States Government as represented by
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
/** @addtogroup libged */
/** @{ */
/** @file libged/ged.cpp
 *
 * A quasi-object-oriented database interface.
 *
 * A database object contains the attributes and methods for
 * controlling a BRL-CAD database.
 *
 * Also include routines to allow libwdb to use librt's import/export
 * interface, rather than having to know about the database formats directly.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>


#include "bu/sort.h"
#include "vmath.h"
#include "bn.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "bv/lod.h"
#include "bv/plot3.h"

#include "bv/defines.h"

#include "./ged_private.h"
#include "./include/plugin.h"

extern "C" void libged_init(void);

extern "C" {
#include "./qray.h"
}

namespace {

constexpr char display_path_separator = '/';
constexpr char display_path_instance = '@';

static bool
simple_display_path(std::string &normalized, const char *path)
{
    if (!path || !path[0] || strchr(path, display_path_instance) ||
	strstr(path, "//"))
	return false;

    normalized.assign(path);
    size_t first = normalized.find_first_not_of(display_path_separator);
    if (first == std::string::npos)
	return false;
    normalized.erase(0, first);

    if (normalized.back() == display_path_separator)
	return false;

    return true;
}


static void
record_display_list_ends(struct ged *gedp)
{
    struct bu_list *head = gedp->i->ged_gdp->gd_headDisplay;
    Ged_Internal *internal = gedp->i->i;

    internal->display_paths_first = head->forw;
    internal->display_paths_last = head->back;
}


static void
sync_display_paths(struct ged *gedp)
{
    struct bu_list *head = gedp->i->ged_gdp->gd_headDisplay;
    Ged_Internal *internal = gedp->i->i;

    if (internal->display_paths_first == head->forw &&
	internal->display_paths_last == head->back)
	return;

    internal->display_paths.clear();
    internal->display_scene_paths.clear();
    internal->display_paths_complex = 0;
    struct display_list *entry;
    for (BU_LIST_FOR(entry, display_list, head)) {
	std::string path;
	if (simple_display_path(path, bu_vls_cstr(&entry->dl_path)))
	    internal->display_paths[path] = entry;
	else
	    internal->display_paths_complex++;
    }
    record_display_list_ends(gedp);
}


static void
insert_display_path(struct ged *gedp, const char *path, struct display_list *entry)
{
    Ged_Internal *internal = gedp->i->i;
    std::string normalized;

    if (simple_display_path(normalized, path))
	internal->display_paths[normalized] = entry;
    else
	internal->display_paths_complex++;
}


static void
remove_display_path(struct ged *gedp, const char *path)
{
    Ged_Internal *internal = gedp->i->i;
    std::string normalized;

    if (simple_display_path(normalized, path)) {
	internal->display_paths.erase(normalized);
    } else if (internal->display_paths_complex) {
	internal->display_paths_complex--;
    }
}


static int
lookup_display_path(struct ged *gedp, const char *path,
                    struct display_list **ancestor, int *exact)
{
    Ged_Internal *internal = gedp->i->i;
    std::string normalized;
    bool exact_match = true;

    *ancestor = NULL;
    if (exact)
        *exact = 0;
    sync_display_paths(gedp);
    if (internal->display_paths_complex ||
	!simple_display_path(normalized, path))
	return 0;

    struct db_full_path validated_path;
    if (db_string_to_path(&validated_path, gedp->dbip, path) != 0)
	return 0;
    db_free_full_path(&validated_path);

    for (;;) {
	std::map<std::string, struct display_list *>::const_iterator match =
	    internal->display_paths.find(normalized);
	if (match != internal->display_paths.end()) {
	    *ancestor = match->second;
	    if (exact)
		*exact = exact_match ? 1 : 0;
	    break;
	}
	exact_match = false;
	const size_t separator = normalized.rfind(display_path_separator);
	if (separator == std::string::npos)
	    break;
	normalized.resize(separator);
    }

    return 1;
}


static std::string
scene_path_key(const struct db_full_path *path)
{
    char *path_string = db_path_to_string(path);
    std::string key;

    if (path_string) {
	key.assign(path_string);
	bu_free(path_string, "display scene path");
    }

    return key;
}


static void
record_scene_list_ends(Ged_Internal::scene_path_index &index,
		       struct display_list *entry)
{
    index.first = entry->dl_head_scene_obj.forw;
    index.last = entry->dl_head_scene_obj.back;
}


static void
rebuild_scene_paths(struct ged *gedp, struct display_list *entry)
{
    Ged_Internal::scene_path_index &index =
	gedp->i->i->display_scene_paths[entry];
    struct bv_scene_obj *sp;

    index.paths.clear();
    for (BU_LIST_FOR(sp, bv_scene_obj, &entry->dl_head_scene_obj)) {
	if (!sp->s_u_data)
	    continue;
	struct ged_bv_data *bdata =
	    (struct ged_bv_data *)sp->s_u_data;
	if (!bdata->s_fullpath.fp_len)
	    continue;
	index.paths.emplace(scene_path_key(&bdata->s_fullpath), sp);
    }
    record_scene_list_ends(index, entry);
}


static Ged_Internal::scene_path_index &
sync_scene_paths(struct ged *gedp, struct display_list *entry)
{
    Ged_Internal *internal = gedp->i->i;
    auto found = internal->display_scene_paths.find(entry);

    if (found == internal->display_scene_paths.end() ||
	found->second.first != entry->dl_head_scene_obj.forw ||
	found->second.last != entry->dl_head_scene_obj.back) {
	rebuild_scene_paths(gedp, entry);
    }

    return internal->display_scene_paths[entry];
}


static int
scene_path_is_descendant(const std::string &candidate,
			 const std::string &path)
{
    return candidate.size() > path.size() &&
	candidate.compare(0, path.size(), path) == 0 &&
	candidate[path.size()] == display_path_separator;
}

}


extern "C" void
_ged_dl_path_sync(struct ged *gedp)
{
    sync_display_paths(gedp);
}


extern "C" int
_ged_dl_path_lookup(struct ged *gedp, const char *path,
                    struct display_list **ancestor, int *exact)
{
    return lookup_display_path(gedp, path, ancestor, exact);
}


extern "C" int
_ged_dl_path_has_related(struct ged *gedp, const char *path)
{
    struct display_list *ancestor = NULL;
    if (!lookup_display_path(gedp, path, &ancestor, NULL) || ancestor)
	return 1;

    std::string normalized;
    if (!simple_display_path(normalized, path))
	return 1;

    normalized.push_back(display_path_separator);
    const std::map<std::string, struct display_list *> &paths = gedp->i->i->display_paths;
    std::map<std::string, struct display_list *>::const_iterator descendant =
	paths.lower_bound(normalized);
    if (descendant == paths.end())
	return 0;

    return descendant->first.compare(0, normalized.size(), normalized) == 0;
}


extern "C" void
_ged_dl_path_insert(struct ged *gedp, const char *path, struct display_list *entry)
{
    insert_display_path(gedp, path, entry);
    record_display_list_ends(gedp);
}


extern "C" void
_ged_dl_path_remove(struct ged *gedp, const char *path)
{
    remove_display_path(gedp, path);
    record_display_list_ends(gedp);
}


extern "C" void
_ged_dl_path_invalidate(struct ged *gedp)
{
    gedp->i->i->display_paths.clear();
    gedp->i->i->display_scene_paths.clear();
    gedp->i->i->display_paths_complex = 0;
    gedp->i->i->display_paths_first = nullptr;
    gedp->i->i->display_paths_last = nullptr;
}


extern "C" void
_ged_dl_path_clear(struct ged *gedp)
{
    gedp->i->i->display_paths.clear();
    gedp->i->i->display_scene_paths.clear();
    gedp->i->i->display_paths_complex = 0;
    record_display_list_ends(gedp);
}


extern "C" int
_ged_dl_scene_path_matches(struct ged *gedp,
			   struct display_list *entry,
			   const struct db_full_path *path,
			   struct bu_ptbl *matches)
{
    if (!gedp || !entry || !path || !matches || !path->fp_len)
	return 0;

    Ged_Internal::scene_path_index &index =
	sync_scene_paths(gedp, entry);
    const std::string key = scene_path_key(path);
    if (key.empty())
	return 0;

    auto exact = index.paths.equal_range(key);
    for (auto i = exact.first; i != exact.second; ++i)
	bu_ptbl_ins(matches, (long *)i->second);

    const std::string prefix = key + display_path_separator;
    auto descendant = index.paths.lower_bound(prefix);
    while (descendant != index.paths.end() &&
	scene_path_is_descendant(descendant->first, key)) {
	bu_ptbl_ins(matches, (long *)descendant->second);
	++descendant;
    }

    return 1;
}


/* Update an existing lazy index after sp is appended at the list tail. */
extern "C" void
_ged_dl_scene_path_insert(struct ged *gedp, struct display_list *entry,
			  struct bv_scene_obj *sp)
{
    if (!gedp || !entry || !sp)
	return;

    Ged_Internal *internal = gedp->i->i;
    auto found = internal->display_scene_paths.find(entry);
    if (found == internal->display_scene_paths.end())
	return;

    struct bu_list *head = &entry->dl_head_scene_obj;
    const void *old_first = (sp->l.back == head) ? head : head->forw;
    const void *old_last = sp->l.back;
    if (found->second.first != old_first ||
	found->second.last != old_last) {
	internal->display_scene_paths.erase(found);
	return;
    }

    if (sp->s_u_data) {
	struct ged_bv_data *bdata =
	    (struct ged_bv_data *)sp->s_u_data;
	if (bdata->s_fullpath.fp_len) {
	    found->second.paths.emplace(
		scene_path_key(&bdata->s_fullpath), sp);
	}
    }
    record_scene_list_ends(found->second, entry);
}


/* Update an existing lazy index after sp is removed from the list. */
extern "C" void
_ged_dl_scene_path_remove(struct ged *gedp, struct display_list *entry,
			  struct bv_scene_obj *sp)
{
    if (!gedp || !entry || !sp)
	return;

    Ged_Internal *internal = gedp->i->i;
    auto found = internal->display_scene_paths.find(entry);
    if (found == internal->display_scene_paths.end())
	return;

    if (sp->s_u_data) {
	struct ged_bv_data *bdata =
	    (struct ged_bv_data *)sp->s_u_data;
	if (bdata->s_fullpath.fp_len) {
	    const std::string key = scene_path_key(&bdata->s_fullpath);
	    auto range = found->second.paths.equal_range(key);
	    for (auto i = range.first; i != range.second; ++i) {
		if (i->second == sp) {
		    found->second.paths.erase(i);
		    break;
		}
	    }
	}
    }
    record_scene_list_ends(found->second, entry);
}


extern "C" void
_ged_dl_scene_path_invalidate(struct ged *gedp,
			      struct display_list *entry)
{
    if (gedp && entry)
	gedp->i->i->display_scene_paths.erase(entry);
}



void
ged_subprocesses_terminate(struct ged *gedp)
{
    if (gedp == GED_NULL)
	return;

    /* Detach application event-loop handlers before freeing their ClientData,
     * then terminate every subprocess.  Removing from the end avoids skipping
     * entries as the table shrinks. */
    while (BU_PTBL_LEN(&gedp->ged_subp)) {
	size_t i = BU_PTBL_LEN(&gedp->ged_subp) - 1;
	struct ged_subprocess *rrp = (struct ged_subprocess *)BU_PTBL_GET(&gedp->ged_subp, i);
	if (gedp->ged_delete_io_handler) {
	    (*gedp->ged_delete_io_handler)(rrp, BU_PROCESS_STDIN);
	    (*gedp->ged_delete_io_handler)(rrp, BU_PROCESS_STDOUT);
	    (*gedp->ged_delete_io_handler)(rrp, BU_PROCESS_STDERR);
	}
	if (!rrp->aborted) {
	    (void)bu_process_terminate(rrp->p);
	    rrp->aborted = 1;
	}
	(void)bu_process_wait_n(&rrp->p, 0);
	bu_ptbl_rm(&gedp->ged_subp, (long *)rrp);
	BU_PUT(rrp, struct ged_subprocess);
    }
    bu_ptbl_reset(&gedp->ged_subp);
}


void
ged_close(struct ged *gedp)
{
    if (gedp == GED_NULL)
	return;

    /* Children and their callbacks must quiesce before either displayed
     * resources or the database they reference are dismantled. */
    ged_subprocesses_terminate(gedp);

    _ged_cmd_completion_cache_clear(gedp);

    if (gedp->dbip) {
	db_close(gedp->dbip);
	gedp->dbip = NULL;
    }

    if (gedp->ged_lod)
	bv_mesh_lod_context_destroy(gedp->ged_lod);

    ged_destroy(gedp);
    gedp = NULL;
}

void
ged_init(struct ged *gedp)
{
    if (gedp == GED_NULL)
	return;

    /* Create internal containers */
    BU_GET(gedp->i, struct ged_impl);
    gedp->i->magic = GED_MAGIC;
    gedp->i->i = new Ged_Internal;

    gedp->dbip = NULL;
    gedp->u_data = NULL;

    // TODO - rename to ged_name
    bu_vls_init(&gedp->go_name);

    // View related containers
    bv_set_init(&gedp->ged_views);
    BU_PTBL_INIT(&gedp->ged_free_views);

    /* TODO: If we're init-ing the list here, does that mean the gedp has
     * ownership of all solid objects created and stored here, and should we
     * then free them when ged_free is called? (don't appear to be currently,
     * just calling FREE_BV_SCENE_OBJ which doesn't de-allocate... */
    BU_PTBL_INIT(&gedp->free_solids);

    // Establish an initial view
    BU_ALLOC(gedp->ged_gvp, struct bview);
    bv_init(gedp->ged_gvp, &gedp->ged_views);
    bu_vls_sprintf(&gedp->ged_gvp->gv_name, "default");
    bv_set_add_view(&gedp->ged_views, gedp->ged_gvp);
    bu_ptbl_ins(&gedp->ged_free_views, (long *)gedp->ged_gvp);

    /* Create a non-opened fbserv */
    BU_GET(gedp->ged_fbs, struct fbserv_obj);
    gedp->ged_fbs->fbs_listener.fbsl_fd = -1;

    BU_GET(gedp->i->ged_gdp, struct ged_drawable);
    BU_GET(gedp->i->ged_gdp->gd_headDisplay, struct bu_list);
    BU_LIST_INIT(gedp->i->ged_gdp->gd_headDisplay);
    BU_GET(gedp->i->ged_gdp->gd_headVDraw, struct bu_list);
    BU_LIST_INIT(gedp->i->ged_gdp->gd_headVDraw);

    gedp->i->ged_gdp->gd_uplotOutputMode = PL_OUTPUT_MODE_BINARY;
    qray_init(gedp->i->ged_gdp);

    BU_GET(gedp->ged_log, struct bu_vls);
    bu_vls_init(gedp->ged_log);

    BU_GET(gedp->ged_results, struct ged_results);
    (void)_ged_results_init(gedp->ged_results);

    BU_PTBL_INIT(&gedp->ged_subp);

    /* For now, we're keeping the string... will go once no one uses it */
    BU_GET(gedp->ged_result_str, struct bu_vls);
    bu_vls_init(gedp->ged_result_str);

    /* Initialize callbacks */
    BU_GET(gedp->ged_cbs, struct ged_callback_state);
    gedp->ged_refresh_handler = NULL;
    gedp->ged_refresh_clientdata = NULL;
    gedp->ged_output_handler = NULL;
    gedp->ged_create_vlist_scene_obj_callback = NULL;
    gedp->ged_create_vlist_display_list_callback = NULL;
    gedp->ged_destroy_vlist_callback = NULL;
    gedp->ged_create_io_handler = NULL;
    gedp->ged_delete_io_handler = NULL;
    gedp->ged_io_data = NULL;

    /* Editor info */
    gedp->app_editors_cnt = 0;
    gedp->app_editors = NULL;
    memset(gedp->editor, '\0', sizeof(gedp->editor));
    BU_PTBL_INIT(&gedp->editor_opts);
    memset(gedp->terminal, '\0', sizeof(gedp->terminal));
    BU_PTBL_INIT(&gedp->terminal_opts);

    /* User data */
    BU_PTBL_INIT(&gedp->ged_uptrs);

    /* ? */
    gedp->ged_output_script = NULL;
    gedp->ged_internal_call = 0;
    gedp->ged_skip_clbks = 0;

    gedp->dbi_state = NULL;

    gedp->ged_interp = NULL;

    gedp->new_cmd_forms = 0;
}

struct ged *
ged_create(void)
{
    struct ged *gedp;
    BU_GET(gedp, struct ged);
    ged_init(gedp);
    return gedp;
}

void
ged_free(struct ged *gedp)
{
    if (!gedp)
	return;

    _ged_cmd_completion_cache_clear(gedp);

    bu_vls_free(&gedp->go_name);

    gedp->ged_gvp = NULL;

    for (size_t i = 0; i < BU_PTBL_LEN(&gedp->ged_free_views); i++) {
	struct bview *gdvp = (struct bview *)BU_PTBL_GET(&gedp->ged_free_views, i);
	bv_free(gdvp);
	bu_free((void *)gdvp, "bv");
    }
    bu_ptbl_free(&gedp->ged_free_views);
    bv_set_free(&gedp->ged_views);

    if (gedp->i->ged_gdp != GED_DRAWABLE_NULL) {

	for (size_t i = 0; i < BU_PTBL_LEN(&gedp->free_solids); i++) {
	    // TODO - FREE_BV_SCENE_OBJ macro is stashing on the free_scene_obj list, not
	    // BU_PUT-ing the solid objects themselves - is that what we expect
	    // when doing ged_free?  I.e., is ownership of the free solid list
	    // with the struct ged or with the application as a whole?  We're
	    // BU_PUT-ing gedp->ged_views.free_scene_obj - above why just that one?
#if 0
	    struct bv_scene_obj *sp = (struct bv_scene_obj *)BU_PTBL_GET(&gedp->free_solids, i);
	    BV_FREE_VLIST(vlfree, &(sp->s_vlist));
#endif
	}
	bu_ptbl_free(&gedp->free_solids);

	if (gedp->i->ged_gdp->gd_headDisplay)
	    BU_PUT(gedp->i->ged_gdp->gd_headDisplay, struct bu_vls);
	if (gedp->i->ged_gdp->gd_headVDraw)
	    BU_PUT(gedp->i->ged_gdp->gd_headVDraw, struct bu_vls);
	qray_free(gedp->i->ged_gdp);
	BU_PUT(gedp->i->ged_gdp, struct ged_drawable);
    }

    if (gedp->ged_log) {
	bu_vls_free(gedp->ged_log);
	BU_PUT(gedp->ged_log, struct bu_vls);
    }

    if (gedp->ged_results) {
	ged_results_free(gedp->ged_results);
	BU_PUT(gedp->ged_results, struct ged_results);
    }

    if (gedp->ged_result_str) {
	bu_vls_free(gedp->ged_result_str);
	BU_PUT(gedp->ged_result_str, struct bu_vls);
    }

    BU_PUT(gedp->ged_cbs, struct ged_callback_state);

    bu_ptbl_free(&gedp->ged_subp);

    if (gedp->ged_fbs)
	BU_PUT(gedp->ged_fbs, struct fbserv_obj);

    bu_ptbl_free(&gedp->editor_opts);
    bu_ptbl_free(&gedp->terminal_opts);
    bu_ptbl_free(&gedp->ged_uptrs);

    /* Free internal containers */
    delete gedp->i->i;
    gedp->i->i = NULL;
    gedp->i->magic = 0;
    BU_PUT(gedp->i, struct ged_impl);
    gedp->i = NULL;;
}

void
ged_destroy(struct ged *gedp)
{
    if (!gedp)
	return;

    ged_free(gedp);
    BU_PUT(gedp, struct ged);
}

struct ged *
ged_open(const char *dbtype, const char *filename, int existing_only)
{
    struct ged *gedp = NULL;
    struct rt_wdb *wdbp = NULL;

    if (filename == NULL)
      return GED_NULL;

    if (BU_STR_EQUAL(dbtype, "db")) {
	struct db_i *dbip;

	if ((dbip = _ged_open_dbip(filename, existing_only)) == DBI_NULL) {
	    return GED_NULL;
	}

	RT_CK_DBI(dbip);

	wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
    } else if (BU_STR_EQUAL(dbtype, "file")) {
	wdbp = wdb_fopen(filename);
    } else {
	struct db_i *dbip;

	/* FIXME: this call should not exist.  passing pointers as
	 * strings indicates a failure in design and lazy coding.
	 */
	if (sscanf(filename, "%p", (void **)&dbip) != 1) {
	    return GED_NULL;
	}

	if (dbip == DBI_NULL) {
	    dbip = db_open_inmem();
	}

	/* Could core dump */
	RT_CK_DBI(dbip);

	if (BU_STR_EQUAL(dbtype, "disk"))
	    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
	else if (BU_STR_EQUAL(dbtype, "disk_append"))
	    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK_APPEND_ONLY);
	else if (BU_STR_EQUAL(dbtype, "inmem"))
	    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
	else if (BU_STR_EQUAL(dbtype, "inmem_append"))
	    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM_APPEND_ONLY);
	else {
	    bu_log("wdb_open %s target type not recognized", dbtype);
	    return GED_NULL;
	}
    }

    gedp = ged_create();
    gedp->dbip = wdbp->dbip;

    db_update_nref(gedp->dbip);

    gedp->ged_lod = NULL;

    return gedp;
}


/**
 * @brief
 * Open/Create the database and build the in memory directory.
 */
struct db_i *
_ged_open_dbip(const char *filename, int existing_only)
{
    struct db_i *dbip = DBI_NULL;

    /* open database */
    if (((dbip = db_open(filename, DB_OPEN_READWRITE)) == DBI_NULL) &&
	((dbip = db_open(filename, DB_OPEN_READONLY)) == DBI_NULL)) {

	/*
	 * Check to see if we can access the database
	 */
	if (bu_file_exists(filename, NULL) && !bu_file_readable(filename)) {
	    bu_log("_ged_open_dbip: %s is not readable", filename);

	    return DBI_NULL;
	}

	if (existing_only)
	    return DBI_NULL;

	/* db_create does a db_dirbuild */
	if ((dbip = db_create(filename, BRLCAD_DB_FORMAT_LATEST)) == DBI_NULL) {
	    bu_log("_ged_open_dbip: failed to create %s\n", filename);

	    return DBI_NULL;
	}

	return dbip;
    }

    /* --- Scan geometry database and build in-memory directory --- */
    if (db_dirbuild(dbip) < 0) {
	db_close(dbip);
	bu_log("_ged_open_dbip: db_dirbuild failed on database file %s", filename);
	dbip = DBI_NULL;
    }

    return dbip;
}

/* Callback wrapper functions */

int
ged_clbk_exec(struct bu_vls *log, struct ged *gedp, int limit, bu_clbk_t f, int ac, const char **av, void *u1, void *u2)
{
    if (!gedp || !f)
	return BRLCAD_ERROR;
    GED_CK_MAGIC(gedp);
    Ged_Internal *gedip = gedp->i->i;
    int rlimit = (limit > 0) ? limit : 1;

    // check depth count before we run clbk
    gedip->clbk_recursion_depth_cnt[f]++;

    if (gedip->clbk_recursion_depth_cnt[f] > rlimit) {
	if (log) {
	    // Print out ged_exec call stack that got us here.  If the
	    // recursion is all in callback functions this won't help, but at
	    // the very least we'll know which ged command to start with.
	    bu_vls_printf(log, "Callback recursion limit %d exceeded.  ged_exec call stack:\n", rlimit);
	    std::stack<std::string> lexec_stack = gedip->exec_stack;
	    while (!lexec_stack.empty()) {
		bu_vls_printf(log, "%s\n", lexec_stack.top().c_str());
		lexec_stack.pop();
	    }
	}
	return BRLCAD_ERROR;
    }

    // Checks complete - actually run the callback
    int ret = (*f)(ac, av, u1, u2);

    // clbk has returned, pop the depth count
    gedip->clbk_recursion_depth_cnt[f]--;

    return ret;
}

void
ged_refresh_cb(struct ged *gedp)
{
    if (gedp->ged_refresh_handler != GED_REFRESH_FUNC_NULL) {
	gedp->ged_cbs->ged_refresh_handler_cnt++;
	if (gedp->ged_cbs->ged_refresh_handler_cnt > 1) {
	    bu_log("Warning - recursive call of gedp->ged_refresh_handler!\n");
	}
	(*gedp->ged_refresh_handler)(gedp->ged_refresh_clientdata);
	gedp->ged_cbs->ged_refresh_handler_cnt--;
    }
}

void
ged_output_handler_cb(struct ged *gedp, char *str)
{
    if (gedp->ged_output_handler != (void (*)(struct ged *, char *))0) {
	gedp->ged_cbs->ged_output_handler_cnt++;
	if (gedp->ged_cbs->ged_output_handler_cnt > 1) {
	    bu_log("Warning - recursive call of gedp->ged_output_handler!\n");
	}
	(*gedp->ged_output_handler)(gedp, str);
	gedp->ged_cbs->ged_output_handler_cnt--;
    }
}

void
ged_create_vlist_solid_cb(struct ged *gedp, struct bv_scene_obj *s)
{
    if (gedp->ged_create_vlist_scene_obj_callback != GED_CREATE_VLIST_SOLID_FUNC_NULL) {
	gedp->ged_cbs->ged_create_vlist_scene_obj_callback_cnt++;
	if (gedp->ged_cbs->ged_create_vlist_scene_obj_callback_cnt > 1) {
	    bu_log("Warning - recursive call of gedp->ged_create_vlist_scene_obj_callback!\n");
	}
	(*gedp->ged_create_vlist_scene_obj_callback)(gedp->vlist_ctx, s);
	gedp->ged_cbs->ged_create_vlist_scene_obj_callback_cnt--;
    }
}

void
ged_create_vlist_display_list_cb(struct ged *gedp, struct display_list *dl)
{
    if (gedp->ged_create_vlist_display_list_callback != GED_CREATE_VLIST_DISPLAY_LIST_FUNC_NULL) {
	gedp->ged_cbs->ged_create_vlist_display_list_callback_cnt++;
	if (gedp->ged_cbs->ged_create_vlist_display_list_callback_cnt > 1) {
	    bu_log("Warning - recursive call of gedp->ged_create_vlist_callback!\n");
	}
	(*gedp->ged_create_vlist_display_list_callback)(gedp->vlist_ctx, dl);
	gedp->ged_cbs->ged_create_vlist_display_list_callback_cnt--;
    }
}

void
ged_destroy_vlist_cb(struct ged *gedp, unsigned int i, int j)
{
    if (gedp->ged_destroy_vlist_callback != GED_DESTROY_VLIST_FUNC_NULL) {
	gedp->ged_cbs->ged_destroy_vlist_callback_cnt++;
	if (gedp->ged_cbs->ged_destroy_vlist_callback_cnt > 1) {
	    bu_log("Warning - recursive call of gedp->ged_destroy_vlist_callback!\n");
	}
	(*gedp->ged_destroy_vlist_callback)(gedp->vlist_ctx, i, j);
	gedp->ged_cbs->ged_destroy_vlist_callback_cnt--;
    }
}

int
ged_clbk_set(struct ged *gedp, const char *cmd_str, int mode, bu_clbk_t f, void *d)
{
    int ret = BRLCAD_OK;
    if (!gedp || !cmd_str)
        return BRLCAD_ERROR;

    GED_CK_MAGIC(gedp);
    Ged_Internal *gedip = gedp->i->i;

    /* Resolve command by name via registry */
    ged_ensure_initialized();
    ged_func_ptr cmd = _ged_cmd_func(cmd_str);
    if (!cmd)
	return (BRLCAD_ERROR | GED_UNKNOWN);

    std::map<ged_func_ptr, std::pair<bu_clbk_t, void *>> *cm =
        (mode == BU_CLBK_PRE) ? &gedip->cmd_prerun_clbk :
        (mode == BU_CLBK_POST) ? &gedip->cmd_postrun_clbk :
        (mode == BU_CLBK_DURING) ? &gedip->cmd_during_clbk :
        &gedip->cmd_linger_clbk;

    auto c_it = cm->find(cmd);
    if (c_it != cm->end())
        ret |= GED_OVERRIDE;

    (*cm)[cmd] = std::make_pair(f, d);
    return ret;
}

int
ged_clbk_get(bu_clbk_t *f, void **d, struct ged *gedp, const char *cmd_str, int mode)
{
    if (!gedp || !cmd_str || !f || !d)
        return BRLCAD_ERROR;

    GED_CK_MAGIC(gedp);
    Ged_Internal *gedip = gedp->i->i;

    /* Resolve command by name via registry */
    ged_ensure_initialized();
    ged_func_ptr cmd = _ged_cmd_func(cmd_str);
    if (!cmd)
        return (BRLCAD_ERROR | GED_UNKNOWN);

    std::map<ged_func_ptr, std::pair<bu_clbk_t, void *>> *cm =
        (mode == BU_CLBK_PRE) ? &gedip->cmd_prerun_clbk :
        (mode == BU_CLBK_POST) ? &gedip->cmd_postrun_clbk :
        (mode == BU_CLBK_DURING) ? &gedip->cmd_during_clbk :
        &gedip->cmd_linger_clbk;

    auto c_it = cm->find(cmd);
    if (c_it == cm->end()) {
        (*f) = NULL;
        (*d) = NULL;
        return BRLCAD_OK;
    }

    (*f) = c_it->second.first;
    (*d) = c_it->second.second;
    return BRLCAD_OK;
}

void
ged_dm_ctx_set(struct ged *gedp, const char *dm_type, void *ctx)
{
    if (!gedp || !dm_type)
	return;

    GED_CK_MAGIC(gedp);
    Ged_Internal *gedip = gedp->i->i;
    gedip->dm_map[std::string(dm_type)] = ctx;
}

void *
ged_dm_ctx_get(struct ged *gedp, const char *dm_type)
{
    if (!gedp || !dm_type)
	return NULL;

    GED_CK_MAGIC(gedp);
    Ged_Internal *gedip = gedp->i->i;
    std::string dm(dm_type);
    if (gedip->dm_map.find(dm) == gedip->dm_map.end())
	return NULL;
    return gedip->dm_map[dm];
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
