/*                        N M G  . C P P
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
/** @file libged/facetize/nmg.cpp
 *
 * Classic NMG boolean evaluation path.
 *
 */

#include "common.h"

#include <string.h>

#include "bio.h"

#include "bu/file.h"
#include "bu/hook.h"
#include "bu/vls.h"
#include "bu/log.h"

#include "../ged_private.h"
#include "./ged_facetize.h"

struct _ged_facetize_logging_state {
    struct bu_hook_list saved_bomb_hooks;
    struct bu_hook_list saved_log_hooks;
    struct bu_vls nmg_log;
    int stderr_stashed;
    int serr;
    int active;
};

extern "C" {
static int
_facetize_bomb_hook(void *cdata, void *str)
{
    struct _ged_facetize_state *o = (struct _ged_facetize_state *)cdata;
    struct _ged_facetize_logging_state *log_s = (struct _ged_facetize_logging_state *)o->log_s;
    bu_vls_printf(&log_s->nmg_log, "%s\n", (const char *)str);
    return 0;
}

static int
_facetize_nmg_logging_hook(void *data, void *str)
{
    struct _ged_facetize_state *o = (struct _ged_facetize_state *)data;
    struct _ged_facetize_logging_state *log_s = (struct _ged_facetize_logging_state *)o->log_s;
    bu_vls_printf(&log_s->nmg_log, "%s\n", (const char *)str);
    return 0;
}

static struct _ged_facetize_logging_state *
_facetize_logging_state_create(struct _ged_facetize_state *o)
{
    if (!o || o->log_s)
	return NULL;

    struct _ged_facetize_logging_state *log_s;
    BU_GET(log_s, struct _ged_facetize_logging_state);
    bu_hook_list_init(&log_s->saved_bomb_hooks);
    bu_hook_list_init(&log_s->saved_log_hooks);
    bu_bomb_save_all_hooks(&log_s->saved_bomb_hooks);
    bu_log_hook_save_all(&log_s->saved_log_hooks);
    bu_vls_init(&log_s->nmg_log);
    log_s->stderr_stashed = -1;
    log_s->serr = -1;
    log_s->active = 0;
    o->log_s = log_s;
    return log_s;
}

static void
_facetize_log_nmg(struct _ged_facetize_state *o)
{
    if (!o || !o->log_s)
	return;

    /* Seriously, bu_bomb, we don't want you blathering
     * to stderr... shut down stderr temporarily. */
    struct _ged_facetize_logging_state *log_s = (struct _ged_facetize_logging_state *)o->log_s;
    log_s->serr = fileno(stderr);
    if (log_s->serr >= 0) {
	int fnull = open(bu_file_null(), O_WRONLY);
	if (fnull != -1) {
	    log_s->stderr_stashed = dup(log_s->serr);
	    if (log_s->stderr_stashed >= 0)
		(void)dup2(fnull, log_s->serr);
	    close(fnull);
	}
    }

    /* Set bu_log logging to capture in nmg_log, rather than the
     * application defaults */
    bu_log_hook_delete_all();
    bu_log_add_hook(_facetize_nmg_logging_hook, (void *)o);

    /* Also engage the nmg bomb hooks */
    bu_bomb_delete_all_hooks();
    bu_bomb_add_hook(_facetize_bomb_hook, (void *)o);
    log_s->active = 1;
}

static void
_facetize_log_default(struct _ged_facetize_state *o)
{
    if (!o || !o->log_s)
	return;

    /* Put stderr back */
    struct _ged_facetize_logging_state *log_s = (struct _ged_facetize_logging_state *)o->log_s;
    if (log_s->stderr_stashed >= 0) {
	fflush(stderr);
	(void)dup2(log_s->stderr_stashed, log_s->serr);
	close(log_s->stderr_stashed);
	log_s->stderr_stashed = -1;
    }

    if (!log_s->active)
	return;

    /* Restore bu_bomb hooks to the application defaults */
    bu_bomb_delete_all_hooks();
    bu_bomb_restore_hooks(&log_s->saved_bomb_hooks);

    /* Restore bu_log hooks to the application defaults */
    bu_log_hook_delete_all();
    bu_log_hook_restore_all(&log_s->saved_log_hooks);
    log_s->active = 0;
}

static void
_facetize_logging_state_destroy(struct _ged_facetize_state *o)
{
    if (!o || !o->log_s)
	return;

    struct _ged_facetize_logging_state *log_s = (struct _ged_facetize_logging_state *)o->log_s;
    _facetize_log_default(o);
    if (bu_vls_strlen(&log_s->nmg_log))
	facetize_log(o, 2, "%s", bu_vls_cstr(&log_s->nmg_log));
    bu_vls_free(&log_s->nmg_log);
    bu_hook_delete_all(&log_s->saved_bomb_hooks);
    bu_hook_delete_all(&log_s->saved_log_hooks);
    BU_PUT(log_s, struct _ged_facetize_logging_state);
    o->log_s = NULL;
}

}

static union tree *
facetize_region_end(struct db_tree_state *tsp,
		    const struct db_full_path *pathp,
		    union tree *curtree,
		    void *client_data)
{
    union tree **facetize_tree;

    if (tsp) RT_CK_DBTS(tsp);
    if (pathp) RT_CK_FULL_PATH(pathp);

    struct _ged_facetize_state *s = (struct _ged_facetize_state *)client_data;
    facetize_tree = &s->facetize_tree;

    if (curtree->tr_op == OP_NOP) return curtree;

    if (*facetize_tree) {
	union tree *tr;
	BU_ALLOC(tr, union tree);
	RT_TREE_INIT(tr);
	tr->tr_op = OP_UNION;
	tr->tr_b.tb_regionp = REGION_NULL;
	tr->tr_b.tb_left = *facetize_tree;
	tr->tr_b.tb_right = curtree;
	*facetize_tree = tr;
    } else {
	*facetize_tree = curtree;
    }

    /* Tree has been saved, and will be freed later */
    return TREE_NULL;
}


static union tree *
facetize_nmg_leaf_tess(struct db_tree_state *tsp, const struct db_full_path *pathp, struct rt_db_internal *ip, void *client_data)
{
    union tree *ret = rt_booltree_leaf_tess(tsp, pathp, ip, NULL);
    if (!ret) {
	struct _ged_facetize_state *s = (struct _ged_facetize_state *)client_data;
	if (s && s->tolerate_failures && pathp) {
	    char *path_str = db_path_to_string(pathp);
	    facetize_tolerated_failure(s, "NMG leaf tessellation failed for '%s'; leaf will be omitted from boolean evaluation", path_str ? path_str : "(unknown)");
	    if (path_str)
		bu_free(path_str, "path string");
	}
    }
    return ret;
}


static struct model *
_try_nmg_facetize(struct _ged_facetize_state *s, struct bu_list *vlfree, int argc, const char **argv)
{
    struct db_i *dbip = s->dbip;
    int i;
    int failed = 0;
    struct db_tree_state init_state;
    union tree *facetize_tree;
    struct model *nmg_model;
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DEFAULT);

    if (!_facetize_logging_state_create(s))
	return NULL;

    _facetize_log_nmg(s);

    db_init_db_tree_state(&init_state, dbip);


    /* Establish tolerances */
    init_state.ts_ttol = &wdbp->wdb_ttol;
    init_state.ts_tol = &wdbp->wdb_tol;

    s->facetize_tree = (union tree *)0;
    facetize_tree = (union tree *)0;
    nmg_model = nmg_mm();
    init_state.ts_m = &nmg_model;

    if (!BU_SETJUMP) {
	/* try */
	i = db_walk_tree(dbip, argc, (const char **)argv,
			 1,
				 &init_state,
				 0,			/* take all regions */
				 facetize_region_end,
				 facetize_nmg_leaf_tess,
				 (void *)s
				);
    } else {
	/* catch */
	BU_UNSETJUMP;
	_facetize_logging_state_destroy(s);
	/* The NMG structures may be inconsistent after a bomb.  Do not walk
	 * them during cleanup and risk a second longjmp outside this guard. */
	s->facetize_tree = NULL;
	return NULL;
    } BU_UNSETJUMP;

    facetize_tree = s->facetize_tree;

    if (i < 0) {
	/* Destroy NMG */
	_facetize_logging_state_destroy(s);
	if (s->facetize_tree) {
	    db_free_tree(s->facetize_tree);
	    s->facetize_tree = NULL;
	}
	nmg_km(nmg_model);
	return NULL;
    }

    if (facetize_tree) {
	if (!BU_SETJUMP) {
	    /* try */
	    failed = nmg_boolean(facetize_tree, nmg_model, vlfree, &wdbp->wdb_tol);
	} else {
	    /* catch */
	    BU_UNSETJUMP;
	    _facetize_logging_state_destroy(s);
	    /* See the db_walk_tree catch above: a bomb invalidates cleanup
	     * assumptions, so abandon this model rather than bombing again. */
	    s->facetize_tree = NULL;
	    return NULL;
	} BU_UNSETJUMP;

    } else {
	failed = 1;
    }

    if (!failed && facetize_tree) {
	NMG_CK_REGION(facetize_tree->tr_d.td_r);
	facetize_tree->tr_d.td_r = (struct nmgregion *)NULL;
    }

    if (failed && s->tolerate_failures) {
	facetize_tolerated_failure(s, "NMG boolean evaluation failed after leaf tessellation; no partial NMG result could be generated for this tree");
    }

    if (facetize_tree) {
	db_free_tree(facetize_tree);
	s->facetize_tree = NULL;
    }

    _facetize_logging_state_destroy(s);
    if (failed)
	nmg_km(nmg_model);
    return (failed) ? NULL : nmg_model;
}

static struct rt_bot_internal *
_try_nmg_to_bot(struct _ged_facetize_state *s, struct model *nmg_model, struct bu_list *vlfree, const struct bn_tol *tol, int *bombed)
{
    struct rt_bot_internal *bot = NULL;

    *bombed = 0;
    if (!_facetize_logging_state_create(s))
	return NULL;
    _facetize_log_nmg(s);
    if (!BU_SETJUMP) {
	bot = (struct rt_bot_internal *)nmg_mdl_to_bot(nmg_model, vlfree, tol);
    } else {
	BU_UNSETJUMP;
	*bombed = 1;
	bot = NULL;
    } BU_UNSETJUMP;
    _facetize_logging_state_destroy(s);

    return bot;
}

static int
_write_nmg(struct _ged_facetize_state *s, struct model *nmg_model, const char *name)
{
    struct db_i *dbip = s->dbip;
    struct rt_db_internal intern;
    struct directory *dp;

    /* Export NMG as a new solid */
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_NMG;
    intern.idb_meth = &OBJ[ID_NMG];
    intern.idb_ptr = (void *)nmg_model;

    dp = db_diradd(dbip, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	if (s->verbosity > 0) {
	    bu_log("Cannot add %s to directory\n", name);
	}
	nmg_km(nmg_model);
	return BRLCAD_ERROR;
    }

    if (rt_db_put_internal(dp, dbip, &intern) < 0) {
	if (s->verbosity > 0) {
	    bu_log("Failed to write %s to database\n", name);
	}
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

static int
_write_bot(struct _ged_facetize_state *s, struct rt_bot_internal *bot, const char *name)
{
    struct db_i *dbip = s->dbip;
    struct rt_db_internal intern;
    struct directory *dp;

    /* Export BoT as a new solid */
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_BOT;
    intern.idb_meth = &OBJ[ID_BOT];
    intern.idb_ptr = (void *)bot;

    dp = db_diradd(dbip, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	if (s->verbosity > 0) {
	    bu_log("Cannot add %s to directory\n", name);
	}
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    if (rt_db_put_internal(dp, dbip, &intern) < 0) {
	if (s->verbosity > 0) {
	    bu_log("Failed to write %s to database\n", name);
	}
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

int
_ged_facetize_nmgeval(struct _ged_facetize_state *s, int argc, const char **argv, const char *oname)
{
    int ret = BRLCAD_OK;
    struct db_i *dbip = s->dbip;
    struct rt_wdb *wdbp;
    struct rt_bot_internal *bot = NULL;
    int conversion_bombed = 0;
    struct bu_list *vlfree = &rt_vlfree;
    struct model *nmg_model = _try_nmg_facetize(s, vlfree, argc, argv);

    if (nmg_model == NULL) {
	if (s->verbosity > 1) {
	    bu_log("NMG(%s):  no resulting region, aborting\n", oname);
	}
	ret = BRLCAD_ERROR;
	goto ged_nmg_obj_memfree;
    }

    if (!s->make_nmg) {

	wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DEFAULT);
	bot = _try_nmg_to_bot(s, nmg_model, vlfree, &wdbp->wdb_tol, &conversion_bombed);
	if (!bot) {
	    ret = BRLCAD_ERROR;
	    if (conversion_bombed)
		nmg_model = NULL;
	    goto ged_nmg_obj_memfree;
	}

	ret = _write_bot(s, bot, oname);
	nmg_km(nmg_model);
	nmg_model = NULL;

    } else {

	/* Write the NMG */
	ret = _write_nmg(s, nmg_model, oname);
	nmg_model = NULL;

    }

ged_nmg_obj_memfree:
    if (nmg_model)
	nmg_km(nmg_model);
    if (s->verbosity >= 0 && ret != BRLCAD_OK) {
	bu_log("NMG: failed to generate %s\n", oname);
    }

    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
