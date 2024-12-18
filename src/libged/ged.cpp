/*                       G E D . C P P
 * BRL-CAD
 *
 * Copyright (c) 2000-2024 United States Government as represented by
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


void
ged_close(struct ged *gedp)
{
    if (gedp == GED_NULL)
	return;

    if (gedp->dbip) {
	db_close(gedp->dbip);
	gedp->dbip = NULL;
    }

    if (gedp->ged_lod)
	bv_mesh_lod_context_destroy(gedp->ged_lod);

    /* Terminate any ged subprocesses */
    if (gedp != GED_NULL) {
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
    }

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

    /* In principle we should be establishing an initial view here,
     * but Archer won't tolerate it. */
    gedp->ged_gvp = GED_VIEW_NULL;

    /* Create a non-opened fbserv */
    BU_GET(gedp->ged_fbs, struct fbserv_obj);
    gedp->ged_fbs->fbs_listener.fbsl_fd = -1;

    BU_GET(gedp->ged_gdp, struct ged_drawable);
    BU_GET(gedp->ged_gdp->gd_headDisplay, struct bu_list);
    BU_LIST_INIT(gedp->ged_gdp->gd_headDisplay);
    BU_GET(gedp->ged_gdp->gd_headVDraw, struct bu_list);
    BU_LIST_INIT(gedp->ged_gdp->gd_headVDraw);

    gedp->ged_gdp->gd_uplotOutputMode = PL_OUTPUT_MODE_BINARY;
    qray_init(gedp->ged_gdp);

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

    /* ? */
    gedp->ged_output_script = NULL;
    gedp->ged_internal_call = 0;

    gedp->dbi_state = NULL;

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
	struct bview *gdvp = (struct bview *)BU_PTBL_GET(&gedp->ged_free_views, i);
	bv_free(gdvp);
	bu_free((void *)gdvp, "bv");
    }
    bu_ptbl_free(&gedp->ged_free_views);
    bv_set_free(&gedp->ged_views);

    if (gedp->ged_gdp != GED_DRAWABLE_NULL) {

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

	if (gedp->ged_gdp->gd_headDisplay)
	    BU_PUT(gedp->ged_gdp->gd_headDisplay, struct bu_vls);
	if (gedp->ged_gdp->gd_headVDraw)
	    BU_PUT(gedp->ged_gdp->gd_headVDraw, struct bu_vls);
	qray_free(gedp->ged_gdp);
	BU_PUT(gedp->ged_gdp, struct ged_drawable);
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
    struct mater *save_materp = MATER_NULL;

    if (filename == NULL)
      return GED_NULL;

    save_materp = rt_material_head();
    rt_new_material_head(MATER_NULL);

    if (BU_STR_EQUAL(dbtype, "db")) {
	struct db_i *dbip;

	if ((dbip = _ged_open_dbip(filename, existing_only)) == DBI_NULL) {
	    /* Restore RT's material head */
	    rt_new_material_head(save_materp);

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
	    /* Restore RT's material head */
	    rt_new_material_head(save_materp);

	    return GED_NULL;
	}

	if (dbip == DBI_NULL) {
	    int i;

	    BU_ALLOC(dbip, struct db_i);
	    dbip->dbi_eof = (b_off_t)-1L;
	    dbip->dbi_fp = NULL;
	    dbip->dbi_mf = NULL;
	    dbip->dbi_read_only = 0;

	    /* Initialize fields */
	    for (i = 0; i <RT_DBNHASH; i++) {
		dbip->dbi_Head[i] = RT_DIR_NULL;
	    }

	    dbip->dbi_local2base = 1.0;		/* mm */
	    dbip->dbi_base2local = 1.0;
	    dbip->dbi_title = bu_strdup("Untitled BRL-CAD Database");
	    dbip->dbi_uses = 1;
	    dbip->dbi_filename = NULL;
	    dbip->dbi_filepath = NULL;
	    dbip->dbi_version = 5;

	    bu_ptbl_init(&dbip->dbi_clients, 128, "dbi_clients[]");
	    bu_ptbl_init(&dbip->dbi_changed_clbks , 8, "dbi_changed_clbks]");
	    bu_ptbl_init(&dbip->dbi_update_nref_clbks, 8, "dbi_update_nref_clbks");

	    dbip->dbi_use_comb_instance_ids = 0;
	    const char *need_comb_inst = getenv("LIBRT_USE_COMB_INSTANCE_SPECIFIERS");
	    if (BU_STR_EQUAL(need_comb_inst, "1")) {
		dbip->dbi_use_comb_instance_ids = 1;
	    }
	    dbip->dbi_magic = DBI_MAGIC;		/* Now it's valid */
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
	    /* Restore RT's material head */
	    rt_new_material_head(save_materp);

	    bu_log("wdb_open %s target type not recognized", dbtype);
	    return GED_NULL;
	}
    }

    gedp = ged_create();
    gedp->dbip = wdbp->dbip;
#if 0
    // Archer doesn't tolerate populating the GEDP with an initial view at the
    // moment.  Probably should fix that, as there are lots of ged commands
    // that only work correctly with a view present...
    BU_ALLOC(gedp->ged_gvp, struct bview);
    bv_init(gedp->ged_gvp, &gedp->ged_views);
    bv_set_add_view(&gedp->ged_views, gedp->ged_gvp);
    bu_ptbl_ins(&gedp->ged_free_views, (long *)gedp->ged_gvp);
#endif

    db_update_nref(gedp->dbip, &rt_uniresource);

    gedp->ged_lod = bv_mesh_lod_context_create(filename);

    const char *use_dbi_state = getenv("LIBGED_DBI_STATE");
    if (use_dbi_state) {
	gedp->dbi_state = new DbiState(gedp);
    } else {
	gedp->dbi_state = NULL;
    }

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

    gedip->clbk_recursion_depth_cnt[f]++;

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

    // Translate the string to a ged pointer.  Commands can have multiple strings
    // associated with the same function - we want to use the cmd pointer as the
    // lookup key, since it will be the same regardless of the string.
    std::map<std::string, const struct ged_cmd *> *cmap = (std::map<std::string, const struct ged_cmd *> *)ged_cmds;
    if (!cmap->size())
	libged_init();
    std::string scmd = std::string(cmd_str);
    std::map<std::string, const struct ged_cmd *>::iterator cm_it = cmap->find(scmd);
    if (cm_it == cmap->end())
	return (BRLCAD_ERROR | GED_UNKNOWN);
    const struct ged_cmd *cmd = cm_it->second;

    std::map<ged_func_ptr, std::pair<bu_clbk_t, void *>> *cm = (mode == GED_CLBK_PRE) ? &gedip->cmd_prerun_clbk : &gedip->cmd_postrun_clbk;
    cm = (mode == GED_CLBK_DURING) ? &gedip->cmd_during_clbk : cm;
    cm = (mode == GED_CLBK_LINGER) ? &gedip->cmd_linger_clbk : cm;
    std::map<ged_func_ptr, std::pair<bu_clbk_t, void *>>::iterator c_it = cm->find(cmd->i->cmd);
    if (c_it != cm->end())
	ret |= GED_OVERRIDE;
    (*cm)[cmd->i->cmd] = std::make_pair(f, d);
    return ret;
}

int
ged_clbk_get(bu_clbk_t *f, void **d, struct ged *gedp, const char *cmd_str, int mode)
{
    if (!gedp || !cmd_str || !f || !d)
	return BRLCAD_ERROR;
    GED_CK_MAGIC(gedp);
    Ged_Internal *gedip = gedp->i->i;

    // Translate the string to a ged pointer.  Commands can have multiple strings
    // associated with the same function - we want to use the cmd pointer as the
    // lookup key, since it will be the same regardless of the string.
    std::map<std::string, const struct ged_cmd *> *cmap = (std::map<std::string, const struct ged_cmd *> *)ged_cmds;
    if (!cmap->size())
	libged_init();
    std::string scmd = std::string(cmd_str);
    std::map<std::string, const struct ged_cmd *>::iterator cm_it = cmap->find(scmd);
    if (cm_it == cmap->end())
	return (BRLCAD_ERROR | GED_UNKNOWN);
    const struct ged_cmd *cmd = cm_it->second;

    std::map<ged_func_ptr, std::pair<bu_clbk_t, void *>> *cm = (mode == GED_CLBK_PRE) ? &gedip->cmd_prerun_clbk : &gedip->cmd_postrun_clbk;
    cm = (mode == GED_CLBK_DURING) ? &gedip->cmd_during_clbk : cm;
    cm = (mode == GED_CLBK_LINGER) ? &gedip->cmd_linger_clbk : cm;
    std::map<ged_func_ptr, std::pair<bu_clbk_t, void *>>::iterator c_it = cm->find(cmd->i->cmd);
    if (c_it == cm->end()) {
	// Nothing set, which is fine - return NULL
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

