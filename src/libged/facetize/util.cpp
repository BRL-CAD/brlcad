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

#include "bu/ptbl.h"
#include "rt/search.h"
#include "../ged_private.h"
#include "./ged_facetize.h"


int
_db_uniq_test(struct bu_vls *n, void *data)
{
    struct ged *gedp = (struct ged *)data;
    if (db_lookup(gedp->dbip, bu_vls_addr(n), LOOKUP_QUIET) == RT_DIR_NULL) return 1;
    return 0;
}

void
_ged_facetize_mkname(struct _ged_facetize_state *s, const char *n, int type)
{
    struct ged *gedp = s->gedp;
    struct bu_vls incr_template = BU_VLS_INIT_ZERO;

    if (type == SOLID_OBJ_NAME) {
	bu_vls_sprintf(&incr_template, "%s%s", n, bu_vls_addr(s->faceted_suffix));
    }
    if (type == COMB_OBJ_NAME) {
	if (s->in_place) {
	    bu_vls_sprintf(&incr_template, "%s_orig", n);
	} else {
	    bu_vls_sprintf(&incr_template, "%s", n);
	}
    }
    if (!bu_vls_strlen(&incr_template)) {
	bu_vls_free(&incr_template);
	return;
    }
    if (db_lookup(gedp->dbip, bu_vls_addr(&incr_template), LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(&incr_template, "-0");
	bu_vls_incr(&incr_template, NULL, NULL, &_db_uniq_test, (void *)gedp);
    }

    if (type == SOLID_OBJ_NAME) {
	bu_avs_add(s->s_map, n, bu_vls_addr(&incr_template));
    }
    if (type == COMB_OBJ_NAME) {
	bu_avs_add(s->c_map, n, bu_vls_addr(&incr_template));
    }

    bu_vls_free(&incr_template);
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

    if (s->in_place && argc > 1) {
	bu_vls_printf(gedp->ged_result_str, "In place conversion specified, but multiple objects listed:\n");
	for (i = 0; i < argc; i++) {
	    bu_vls_printf(gedp->ged_result_str, "       %s\n", argv[i]);
	}
	bu_vls_printf(gedp->ged_result_str, "\nAborting.  An in-place conversion replaces the specified object (or regions in a hierarchy in -r mode) with a faceted approximation.  Because a single object is generated, in-place conversion has no clear interpretation when m  ultiple input objects are specified.\n");
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
_ged_check_plate_mode(struct ged *gedp, struct directory *dp)
{
    unsigned int i;
    int ret = 0;
    struct bu_ptbl *bot_dps = NULL;
    const char *bot_objs = "-type bot";
    if (!dp || !gedp) return 0;

    BU_ALLOC(bot_dps, struct bu_ptbl);
    if (db_search(bot_dps, DB_SEARCH_RETURN_UNIQ_DP, bot_objs, 1, &dp, gedp->dbip, NULL) < 0) {
	goto ged_check_plate_mode_memfree;
    }

    /* Got all the BoT objects in the tree, check each of them for validity */
    for (i = 0; i < BU_PTBL_LEN(bot_dps); i++) {
	struct rt_db_internal intern;
	struct rt_bot_internal *bot;
	struct directory *bot_dp = (struct directory *)BU_PTBL_GET(bot_dps, i);
	GED_DB_GET_INTERNAL(gedp, &intern, bot_dp, bn_mat_identity, &rt_uniresource, BRLCAD_ERROR);
	bot = (struct rt_bot_internal *)intern.idb_ptr;
	RT_BOT_CK_MAGIC(bot);
	if (bot->mode == RT_BOT_PLATE || bot->mode == RT_BOT_PLATE_NOCOS) {
	    ret = 1;
	    rt_db_free_internal(&intern);
	    goto ged_check_plate_mode_memfree;
	}
	rt_db_free_internal(&intern);
    }

ged_check_plate_mode_memfree:
    bu_ptbl_free(bot_dps);
    bu_free(bot_dps, "bot directory pointer table");
    return ret;
}


int
_ged_facetize_verify_solid(struct _ged_facetize_state *s, int argc, struct directory **dpa)
{
    struct ged *gedp = s->gedp;
    unsigned int i;
    int ret = 1;
    struct bu_ptbl *bot_dps = NULL;
    const char *bot_objs = "-type bot";
    const char *pnt_objs = "-type pnts";
    if (argc < 1 || !dpa || !gedp) return 0;

    /* If we have pnts, it's not a solid tree */
    if (db_search(NULL, DB_SEARCH_QUIET, pnt_objs, argc, dpa, gedp->dbip, NULL) > 0) {
	if (s->verbosity) {
	    bu_log("-- Found pnts objects in tree\n");
	}
	return 0;
    }

    BU_ALLOC(bot_dps, struct bu_ptbl);
    if (db_search(bot_dps, DB_SEARCH_RETURN_UNIQ_DP, bot_objs, argc, dpa, gedp->dbip, NULL) < 0) {
	if (s->verbosity) {
	    bu_log("Problem searching for BoTs - aborting.\n");
	}
	ret = 0;
	goto ged_facetize_verify_objs_memfree;
    }

    /* Got all the BoT objects in the tree, check each of them for validity */
    for (i = 0; i < BU_PTBL_LEN(bot_dps); i++) {
	struct rt_db_internal intern;
	struct rt_bot_internal *bot;
	int not_solid;
	struct directory *bot_dp = (struct directory *)BU_PTBL_GET(bot_dps, i);
	GED_DB_GET_INTERNAL(gedp, &intern, bot_dp, bn_mat_identity, &rt_uniresource, BRLCAD_ERROR);
	bot = (struct rt_bot_internal *)intern.idb_ptr;
	RT_BOT_CK_MAGIC(bot);
	if (bot->mode != RT_BOT_PLATE && bot->mode != RT_BOT_PLATE_NOCOS) {
	    not_solid = bg_trimesh_solid2((int)bot->num_vertices, (int)bot->num_faces, bot->vertices, bot->faces, NULL);
	    if (not_solid) {
		if (s->verbosity) {
		    bu_log("-- Found non solid BoT: %s\n", bot_dp->d_namep);
		}
		ret = 0;
	    }
	}
	rt_db_free_internal(&intern);
    }

    /* TODO - any other objects that need a pre-boolean validity check? */

ged_facetize_verify_objs_memfree:
    bu_ptbl_free(bot_dps);
    bu_free(bot_dps, "bot directory pointer table");
    return ret;
}

int
_ged_facetize_obj_swap(struct ged *gedp, const char *o, const char *n)
{
    int ret = BRLCAD_OK;
    const char *mav[3];
    const char *cmdname = "facetize";
    /* o or n may point to a d_namep location, which will change with
     * moves, so copy them up front for consistent values */
    struct bu_vls oname = BU_VLS_INIT_ZERO;
    struct bu_vls nname = BU_VLS_INIT_ZERO;
    struct bu_vls tname = BU_VLS_INIT_ZERO;
    mav[0] = cmdname;
    bu_vls_sprintf(&oname, "%s", o);
    bu_vls_sprintf(&nname, "%s", n);
    bu_vls_sprintf(&tname, "%s", o);
    bu_vls_incr(&tname, NULL, "0:0:0:0:-", &_db_uniq_test, (void *)gedp);
    mav[1] = bu_vls_addr(&oname);
    mav[2] = bu_vls_addr(&tname);
    if (ged_exec(gedp, 3, (const char **)mav) != BRLCAD_OK) {
	ret = BRLCAD_ERROR;
	goto ged_facetize_obj_swap_memfree;
    }
    mav[1] = bu_vls_addr(&nname);
    mav[2] = bu_vls_addr(&oname);
    if (ged_exec(gedp, 3, (const char **)mav) != BRLCAD_OK) {
	ret = BRLCAD_ERROR;
	goto ged_facetize_obj_swap_memfree;
    }
    mav[1] = bu_vls_addr(&tname);
    mav[2] = bu_vls_addr(&nname);
    if (ged_exec(gedp, 3, (const char **)mav) != BRLCAD_OK) {
	ret = BRLCAD_ERROR;
	goto ged_facetize_obj_swap_memfree;
    }

ged_facetize_obj_swap_memfree:
    bu_vls_free(&oname);
    bu_vls_free(&nname);
    bu_vls_free(&tname);
    return ret;
}

int
_ged_facetize_write_bot(struct _ged_facetize_state *s, struct rt_bot_internal *bot, const char *name)
{
    struct ged *gedp = s->gedp;
    struct db_i *dbip = gedp->dbip;

    /* Export BOT as a new solid */
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_BOT;
    intern.idb_meth = &OBJ[ID_BOT];
    intern.idb_ptr = (void *)bot;

    struct directory *dp = db_diradd(dbip, name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type);
    if (dp == RT_DIR_NULL) {
	if (s->verbosity)
	    bu_log("Cannot add %s to directory\n", name);
	return BRLCAD_ERROR;
    }

    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	if (s->verbosity)
	    bu_log("Failed to write %s to database\n", name);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

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

