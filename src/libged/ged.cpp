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
#include "bu/hash.h"
#include "vmath.h"
#include "bn.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "bsg/lod.h"
#include "bsg/plot3.h"
#include "dm.h"

#include "bsg/defines.h"
#include "bsg/node.h"
#include "bsg/util.h"
#include "ged/bsg_ged_draw.h"
#include "ged/event_txn.h"
#include "./ged_private.h"
#include "./include/plugin.h"

extern "C" void libged_init(void);

extern "C" {
#include "./qray.h"
}

static void
ged_old_selection_set_free(struct rt_selection_set *set)
{
    if (!set)
	return;

    if (set->free_selection) {
	for (size_t i = 0; i < BU_PTBL_LEN(&set->selections); i++) {
	    struct rt_selection *selection = (struct rt_selection *)BU_PTBL_GET(&set->selections, i);
	    if (selection)
		set->free_selection(selection);
	}
    }

    bu_ptbl_free(&set->selections);
    BU_FREE(set, struct rt_selection_set);
}

static void
ged_old_object_selections_free(struct rt_object_selections *obj_selections)
{
    if (!obj_selections)
	return;

    if (obj_selections->sets) {
	struct bu_hash_entry *entry = NULL;
	while ((entry = bu_hash_next(obj_selections->sets, entry)) != NULL) {
	    struct rt_selection_set *set = (struct rt_selection_set *)bu_hash_value(entry, NULL);
	    ged_old_selection_set_free(set);
	}
	bu_hash_destroy(obj_selections->sets);
	obj_selections->sets = NULL;
    }

    BU_FREE(obj_selections, struct rt_object_selections);
}

static void
ged_old_selections_free(struct ged *gedp)
{
    if (!gedp || !gedp->ged_selections)
	return;

    struct bu_hash_entry *entry = NULL;
    while ((entry = bu_hash_next(gedp->ged_selections, entry)) != NULL) {
	struct rt_object_selections *obj_selections = (struct rt_object_selections *)bu_hash_value(entry, NULL);
	ged_old_object_selections_free(obj_selections);
    }

    bu_hash_destroy(gedp->ged_selections);
    gedp->ged_selections = NULL;
}


void
ged_close(struct ged *gedp)
{
    if (gedp == GED_NULL)
	return;

    /* Clear all displayed geometry BEFORE closing the database.
     * Scene objects hold directory pointers that are only valid while dbip is
     * open; closing dbip first causes use-after-free during BSG / dl_*
     * scene-object teardown.  ged_close_core() in close/close.cpp already
     * follows this order — this function must match it. */
    if (gedp->dbip) {
	const char *av[1] = {"zap"};
	ged_exec_zap(gedp, 1, (const char **)av);
    }

    ged_event_librt_callbacks_disable(gedp);

    if (gedp->dbip) {
	db_close(gedp->dbip);
	gedp->dbip = NULL;
    }

    if (gedp->ged_lod) {
	bsg_mesh_lod_context_destroy(gedp->ged_lod);
	gedp->ged_lod = NULL;
    }

    /* Terminate any ged subprocesses */
    for (size_t i = 0; i < BU_PTBL_LEN(&gedp->ged_subp); i++) {
	struct ged_subprocess *rrp = (struct ged_subprocess *)BU_PTBL_GET(&gedp->ged_subp, i);
	if (!rrp->aborted) {
	    bu_pid_terminate(bu_process_pid(rrp->p));
	    rrp->aborted = 1;
	}
	bu_ptbl_rm(&gedp->ged_subp, (long *)rrp);
	BU_PUT(rrp, struct ged_subprocess);
    }
    bu_ptbl_reset(&gedp->ged_subp);

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
    gedp->i->ged_db_indexp = ged_db_index_create(gedp);
    gedp->i->ged_event_txnp = ged_event_txn_state_create(gedp);
    gedp->i->ged_selection_statep = ged_selection_state_create(gedp);

    gedp->dbip = NULL;
    gedp->u_data = NULL;

    // TODO - rename to ged_name
    bu_vls_init(&gedp->go_name);

    // View related containers
    bsg_set_init(&gedp->ged_views);
    BU_PTBL_INIT(&gedp->ged_free_views);

    // Establish an initial view
    BU_ALLOC(gedp->ged_gvp, struct bsg_view);
    bsg_init(gedp->ged_gvp, &gedp->ged_views);
    bu_vls_sprintf(&gedp->ged_gvp->gv_name, "default");
    bsg_set_add_view(&gedp->ged_views, gedp->ged_gvp);
    bu_ptbl_ins(&gedp->ged_free_views, (long *)gedp->ged_gvp);

    /* Create a non-opened fbserv */
    BU_GET(gedp->ged_fbs, struct fbserv_obj);
    gedp->ged_fbs->fbs_listener.fbsl_fd = -1;

    BU_GET(gedp->i->ged_gdp, struct ged_drawable);
    ged_scene_root_ref_clear(gedp);
    BU_PTBL_INIT(&gedp->i->ged_gdp->gd_draw_registry);
    gedp->i->ged_gdp->gd_draw_registry_init = 1;
    gedp->i->ged_gdp->gd_draw_next_token = 1;
    gedp->i->ged_gdp->gd_draw_shapes_by_component_hash = NULL;
    gedp->i->ged_gdp->gd_draw_shapes_by_path_hash = NULL;
    gedp->i->ged_gdp->gd_draw_groups_by_component_hash = NULL;
    gedp->i->ged_gdp->gd_draw_groups_by_path_hash = NULL;
    gedp->i->ged_gdp->gd_draw_index_shape_component_queries = 0;
    gedp->i->ged_gdp->gd_draw_index_shape_component_candidates = 0;
    gedp->i->ged_gdp->gd_draw_index_group_component_queries = 0;
    gedp->i->ged_gdp->gd_draw_index_group_component_candidates = 0;
    gedp->i->ged_gdp->gd_draw_index_path_queries = 0;
    gedp->i->ged_gdp->gd_draw_index_path_candidates = 0;
    gedp->i->ged_gdp->gd_draw_index_fallback_shape_scans = 0;
    gedp->i->ged_gdp->gd_draw_index_fallback_group_scans = 0;
    BU_PTBL_INIT(&gedp->i->ged_gdp->gd_draw_observers);
    gedp->i->ged_gdp->gd_draw_observers_init = 1;
    gedp->i->ged_gdp->gd_draw_next_observer_token = 1;
    gedp->i->ged_gdp->gd_draw_observer_dispatch_depth = 0;
    /* Start at 1 so that freshly-drawn shapes (s_color_rev=0 from calloc)
     * are always stale on the first color_from_soltab call (B4). */
    gedp->i->ged_gdp->gd_mater_rev = 1;
    BU_GET(gedp->i->ged_gdp->gd_headVDraw, struct bu_list);
    BU_LIST_INIT(gedp->i->ged_gdp->gd_headVDraw);

    gedp->i->ged_gdp->gd_uplotOutputMode = PL_OUTPUT_MODE_BINARY;
    qray_init(gedp->i->ged_gdp);

    /* Eagerly create the draw root so GED_CHECK_DRAWABLE succeeds */
    ged_draw_ensure_root(gedp);

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

    gedp->ged_selections = NULL;

    /* ? */
    gedp->ged_output_script = NULL;
    gedp->ged_internal_call = 0;
    gedp->ged_skip_clbks = 0;

    gedp->ged_interp = NULL;

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

    bu_vls_free(&gedp->go_name);

    gedp->ged_gvp = NULL;

    for (size_t i = 0; i < BU_PTBL_LEN(&gedp->ged_free_views); i++) {
	struct bsg_view *gdvp = (struct bsg_view *)BU_PTBL_GET(&gedp->ged_free_views, i);
	bsg_free(gdvp);
	bu_free((void *)gdvp, "bv");
    }
    bu_ptbl_free(&gedp->ged_free_views);
    bsg_set_free(&gedp->ged_views);

    if (gedp->i->ged_gdp != GED_DRAWABLE_NULL) {

	ged_scene_root_ref_clear(gedp);  /* freed by zap */
	if (gedp->i->ged_gdp->gd_headVDraw) {
	    struct vd_curve *curve = NULL;
	    while (BU_LIST_WHILE(curve, vd_curve, gedp->i->ged_gdp->gd_headVDraw)) {
		BU_LIST_DEQUEUE(&curve->l);
		if (curve->vdc_points)
		    bu_free(curve->vdc_points, "vdraw points");
		if (curve->vdc_commands)
		    bu_free(curve->vdc_commands, "vdraw commands");
		BU_PUT(curve, struct vd_curve);
	    }
	    BU_PUT(gedp->i->ged_gdp->gd_headVDraw, struct bu_list);
	}
		qray_free(gedp->i->ged_gdp);
		ged_draw_observers_free(gedp);
		ged_draw_registry_free(gedp);
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

    ged_old_selections_free(gedp);

    if (gedp->i && gedp->i->ged_db_indexp) {
	ged_db_index_destroy(gedp->i->ged_db_indexp);
	gedp->i->ged_db_indexp = NULL;
    }
    if (gedp->i && gedp->i->ged_event_txnp) {
	ged_event_txn_state_destroy(gedp->i->ged_event_txnp);
	gedp->i->ged_event_txnp = NULL;
    }
    if (gedp->i && gedp->i->ged_selection_statep) {
	ged_selection_state_destroy(gedp->i->ged_selection_statep);
	gedp->i->ged_selection_statep = NULL;
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
    ged_event_librt_callbacks_enable(gedp);

    db_update_nref(gedp->dbip);

    gedp->ged_lod = bsg_mesh_lod_context_create(filename);

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
    bu_plugin_cmd_impl cmd = bu_plugin_cmd_get(cmd_str);
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
    bu_plugin_cmd_impl cmd = bu_plugin_cmd_get(cmd_str);
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

extern "C" GED_EXPORT void
ged_rt_fb_set(struct ged *gedp, const char *fb_dev)
{
    if (!gedp)
	return;

    GED_CK_MAGIC(gedp);
    Ged_Internal *gedip = gedp->i->i;
    gedip->rt_fb_dev = (fb_dev) ? std::string(fb_dev) : std::string();
}

extern "C" GED_EXPORT const char *
ged_rt_fb_get(struct ged *gedp)
{
    if (!gedp)
	return NULL;

    GED_CK_MAGIC(gedp);
    Ged_Internal *gedip = gedp->i->i;
    if (gedip->rt_fb_dev.empty())
	return NULL;
    return gedip->rt_fb_dev.c_str();
}

extern "C" GED_EXPORT void
ged_rt_fb_refresh(struct ged *gedp)
{
    const char *dm_name = NULL;

    if (!gedp || !gedp->ged_gvp || !gedp->ged_gvp->dmp)
	return;

    GED_CK_MAGIC(gedp);
    dm_name = dm_get_dm_name((struct dm *)gedp->ged_gvp->dmp);
    if (!dm_name)
	return;

    if (BU_STR_EQUAL(dm_name, "swrast")) { ged_rt_fb_set(gedp, "/dev/swrast"); return; }
    if (BU_STR_EQUAL(dm_name, "qtgl")) { ged_rt_fb_set(gedp, "/dev/qtgl"); return; }
    if (BU_STR_EQUAL(dm_name, "ogl")) { ged_rt_fb_set(gedp, "/dev/ogl"); return; }
    if (BU_STR_EQUAL(dm_name, "wgl")) { ged_rt_fb_set(gedp, "/dev/wgl"); return; }
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
