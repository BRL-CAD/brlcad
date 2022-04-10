/*                         E R A S E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @file libged/erase.cpp
 *
 * The erase command.
 *
 */

#include "common.h"

#include <set>

#include <stdlib.h>
#include <string.h>

#include "bu/sort.h"
#include "ged/view/state.h"

#define ALPHANUM_IMPL
#include "../alphanum.h"
#include "../ged_private.h"

#define GET_BV_SCENE_OBJ(p, fp) { \
	if (BU_LIST_IS_EMPTY(fp)) { \
	    BU_ALLOC((p), struct bv_scene_obj); \
	} else { \
	    p = BU_LIST_NEXT(bv_scene_obj, fp); \
	    BU_LIST_DEQUEUE(&((p)->l)); \
	} \
	BU_LIST_INIT( &((p)->s_vlist) ); }

// Need to process shallowest to deepest, so we properly split new scene groups
// generated from higher level paths that are in turn split by other deeper
// paths.
struct dfp_cmp {
    bool operator() (struct db_full_path *fp, struct db_full_path *o) const {
	// First, check length
	if (fp->fp_len != o->fp_len) {
	    return (fp->fp_len < o->fp_len);
	}

	// If length is the same, check the dp contents
	for (size_t i = 0; i < fp->fp_len; i++) {
	    if (!BU_STR_EQUAL(fp->fp_names[i]->d_namep, o->fp_names[i]->d_namep)) {
		return (bu_strcmp(fp->fp_names[i]->d_namep, o->fp_names[i]->d_namep) < 0);
	    }
	}
	return (bu_strcmp(fp->fp_names[fp->fp_len-1]->d_namep, o->fp_names[fp->fp_len-1]->d_namep) < 0);
    }
};

/* Return 1 if we did a split, 0 otherwise */
int
path_add_children(std::set<struct bv_scene_group *> *ngrps, struct db_i *dbip, struct db_full_path *gfp, struct db_full_path *fp, struct bv_scene_obj *free_scene_obj)
{
    // Get the list of immediate children from gfp:
    struct directory **children = NULL;
    struct rt_db_internal in;
    struct rt_comb_internal *comb;
    if (rt_db_get_internal(&in, DB_FULL_PATH_CUR_DIR(gfp), dbip, NULL, &rt_uniresource) < 0) {
	// Invalid comb
	return 0;
    }
    comb = (struct rt_comb_internal *)in.idb_ptr;
    db_comb_children(dbip, comb, &children, NULL, NULL);

    // First, make sure fp actually is a path matching one of this comb's children.
    // A prior top match is not actually a guarantee of a full match.
    int i = 0;
    int path_match = 0;
    struct directory *dp =  children[0];
    while (dp != RT_DIR_NULL) {
	db_add_node_to_full_path(gfp, dp);
	if (db_full_path_match_top(gfp, fp)) {
	    path_match = 1;
	    DB_FULL_PATH_POP(gfp);
	    break;
	}
	i++;
	dp = children[i];
	DB_FULL_PATH_POP(gfp);
    }

    if (!path_match) {
	return 0;
    }

    i = 0;
    dp =  children[0];
    while (dp != RT_DIR_NULL) {
	db_add_node_to_full_path(gfp, dp);
	if (db_full_path_match_top(gfp, fp)) {
	    if (DB_FULL_PATH_CUR_DIR(gfp) != DB_FULL_PATH_CUR_DIR(fp))
		path_add_children(ngrps, dbip, gfp, fp, free_scene_obj);
	} else {
	    struct bu_vls pvls = BU_VLS_INIT_ZERO;
	    struct bv_scene_group *g;
	    GET_BV_SCENE_OBJ(g, &free_scene_obj->l);
	    BU_LIST_INIT(&(g->s_vlist));
	    BU_PTBL_INIT(&g->children);
	    bu_vls_trunc(&pvls, 0);
	    db_path_to_vls(&pvls, gfp);
	    BU_VLS_INIT(&g->s_name);
	    bu_vls_sprintf(&g->s_name, "%s", bu_vls_cstr(&pvls));
	    BU_VLS_INIT(&g->s_uuid);
	    bu_vls_sprintf(&g->s_uuid, "%s", bu_vls_cstr(&pvls));
	    ngrps->insert(g);
	}
	i++;
	dp = children[i];
	DB_FULL_PATH_POP(gfp);
    }

    rt_db_free_internal(&in);
    bu_free(children, "free children struct directory ptr array");

    return 1;
}

void
new_scene_grps(std::set<struct bv_scene_group *> *all, struct db_i *dbip, struct bv_scene_group *cg, std::set<std::string> &spaths, struct bv_scene_obj *free_scene_obj)
{
    std::set<struct db_full_path *, dfp_cmp> sfp;
    std::set<std::string>::iterator s_it;

    // Turn spaths into db_full_paths so we can do depth ordered processing
    for (s_it = spaths.begin(); s_it != spaths.end(); s_it++) {
	struct db_full_path *fp = NULL;
	BU_GET(fp, struct db_full_path);
	db_full_path_init(fp);
	int ret = db_string_to_path(fp, dbip, s_it->c_str());
	if (ret < 0) {
	    // Invalid path
	    db_free_full_path(fp);
	    continue;
	}
	sfp.insert(fp);
    }

    // Remove the children of cg from it's ptbl for later processing so we
    // don't have to special case freeing cg
    std::set<struct bv_scene_obj *> sobjs;
    for (size_t j = 0; j < BU_PTBL_LEN(&cg->children); j++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(&cg->children, j);
	sobjs.insert(s);
    }
    bu_ptbl_reset(&cg->children);

    // Seed our group set with the top level group
    std::set<struct bv_scene_group *> ngrps;
    ngrps.insert(cg);

    // Now, work our way through sfp.  For each sfp path, split everything
    // in ngrps that matches it.
    std::set<struct bv_scene_group *>::iterator g_it;
    while (sfp.size()) {
	struct db_full_path *fp = *sfp.begin();
	sfp.erase(sfp.begin());

	std::set<struct bv_scene_group *> gclear;
	std::set<struct bv_scene_group *> next_grps;

	for (g_it = ngrps.begin(); g_it != ngrps.end(); g_it++) {
	    struct bv_scene_group *ng = *g_it;
	    struct db_full_path *gfp = NULL;
	    BU_GET(gfp, struct db_full_path);
	    db_full_path_init(gfp);
	    int ret = db_string_to_path(gfp, dbip, bu_vls_cstr(&ng->s_name));
	    if (ret < 0) {
		// Invalid path
		db_free_full_path(gfp);
		continue;
	    }
	    if (db_full_path_match_top(gfp, fp)) {
		// Matches.  Make new groups based on the children, and
		// eliminate the gfp group
		if (path_add_children(&next_grps, dbip, gfp, fp, free_scene_obj)) {
		    gclear.insert(ng);
		}
	    }
	}

	// Free up the replaced groups
	for (g_it = gclear.begin(); g_it != gclear.end(); g_it++) {
	    struct bv_scene_group *ng = *g_it;
	    ngrps.erase(ng);
	    bv_scene_obj_free(ng, free_scene_obj);
	    BU_PUT(ng, struct bv_scene_group);
	}

	// Populate ngrps for the next round
	for (g_it = next_grps.begin(); g_it != next_grps.end(); g_it++) {
	    ngrps.insert(*g_it);
	}

    }

    // All paths processed - ngrps should now contain the final set of groups to
    // add to the view.  Assign the solids
    std::set<struct bv_scene_obj *>::iterator sg_it;
    for (sg_it = sobjs.begin(); sg_it != sobjs.end(); sg_it++) {
	struct bv_scene_obj *sobj = *sg_it;
	struct draw_update_data_t *ud = (struct draw_update_data_t *)sobj->s_i_data;
	// Find the correct group
	for (g_it = ngrps.begin(); g_it != ngrps.end(); g_it++) {
	    struct bv_scene_group *ng = *g_it;
	    struct db_full_path *gfp = NULL;
	    BU_GET(gfp, struct db_full_path);
	    db_full_path_init(gfp);
	    int ret = db_string_to_path(gfp, dbip, bu_vls_cstr(&ng->s_name));
	    if (ret < 0) {
		// Invalid path
		db_free_full_path(gfp);
		continue;
	    }
	    if (db_full_path_match_top(gfp, &ud->fp)) {
		bu_ptbl_ins(&ng->children, (long *)sobj);
		break;
	    }
	}
    }

    // Put the new groups in the view group set
    for (g_it = ngrps.begin(); g_it != ngrps.end(); g_it++) {
	struct bv_scene_group *ng = *g_it;
	all->insert(ng);
    }
}

static int
alphanum_cmp(const void *a, const void *b, void *UNUSED(data)) {
    struct bv_scene_group *ga = *(struct bv_scene_group **)a;
    struct bv_scene_group *gb = *(struct bv_scene_group **)b;
    return alphanum_impl(bu_vls_cstr(&ga->s_name), bu_vls_cstr(&gb->s_name), NULL);
}

/*
 * Erase objects from the display.
 *
 * TODO - like draw2, this needs to be aware of whether we're using local or shared
 * grp sets.
 */
extern "C" int
ged_erase2_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "[object(s)]";
    const char *cmdName = argv[0];
    struct bview *v = gedp->ged_gvp;
    struct db_i *dbip = gedp->dbip;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Erase may operate on a specific user specified view.  If it does so,
     * we want the default settings to reflect those set in that particular
     * view.  In order to set up the correct default views, we need to know
     * if a specific view has in fact been specified.  We do a preliminary
     * option check to figure this out */
    struct bu_vls cvls = BU_VLS_INIT_ZERO;
    struct bu_opt_desc vd[2];
    BU_OPT(vd[0],  "V", "view",    "name",      &bu_opt_vls, &cvls,   "specify view to draw on");
    BU_OPT_NULL(vd[1]);
    int opt_ret = bu_opt_parse(NULL, argc, argv, vd);
    argc = opt_ret;
    if (bu_vls_strlen(&cvls)) {
	int found_match = 0;
	for (size_t i = 0; i < BU_PTBL_LEN(&gedp->ged_views.views); i++) {
	    struct bview *tv = (struct bview *)BU_PTBL_GET(&gedp->ged_views.views, i);
	    if (BU_STR_EQUAL(bu_vls_cstr(&tv->gv_name), bu_vls_cstr(&cvls))) {
		v = tv;
		found_match = 1;
		break;
	    }
	}
	if (!found_match) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s not found\n", bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}

	if (!v->independent) {
	    bu_vls_printf(gedp->ged_result_str, "Specified view %s is not an independent view, and as such does not support specifying db objects for display in only this view.  To change the view's status, he   command 'view independent %s 1' may be applied.\n", bu_vls_cstr(&cvls), bu_vls_cstr(&cvls));
	    bu_vls_free(&cvls);
	    return BRLCAD_ERROR;
	}
    }
    bu_vls_free(&cvls);


    /* Check that we have a view */
    if (!v) {
	bu_vls_printf(gedp->ged_result_str, "No view specified and no current view defined in GED, nothing to erase from");
	return BRLCAD_ERROR;
    }

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmdName, usage);
	return BRLCAD_HELP;
    }

    /* skip past cmd */
    argc--; argv++;

    // Check the supplied paths against the drawn paths.  If they are equal or
    // if the supplied path contains a drawn path, then we just completely
    // eliminate the group.  If the supplied path is a subset of a drawn path,
    // we're removing only part of the group and have to split the drawn path.
    //
    // The operation we want to employ here is db_full_path_match_top, but at
    // this stage of erase processing we don't know that either the supplied
    // or the drawn paths are valid.  Anything that is invalid is erased, but
    // until it is we can't turn strings into paths reliably.  So we use the
    // bu_vls_strncmp function, and prepend a "/" character in the case whare a
    // user supplies a name without the path prefix.
    //
    struct bv_scene_obj *free_scene_obj = gedp->ged_views.free_scene_obj;
    struct bu_ptbl *sg = (v->independent || v->gv_s->adaptive_plot) ? v->gv_objs.view_grps : &v->vset->shared_db_objs;
    std::set<struct bv_scene_group *> all;
    for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
	struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(sg, i);
	all.insert(cg);
    }
    bu_ptbl_reset(sg);

    std::set<struct bv_scene_group *> clear;
    std::set<struct bv_scene_group *>::iterator c_it;
    std::map<struct bv_scene_group *, std::set<std::string>> split;
    std::map<struct bv_scene_group *, std::set<std::string>>::iterator g_it;
    struct bu_vls upath = BU_VLS_INIT_ZERO;
    for (int i = 0; i < argc; ++i) {
	bu_vls_sprintf(&upath, "%s", argv[i]);
	if (argv[i][0] != '/') {
	    bu_vls_prepend(&upath, "/");
	}
	for (c_it = all.begin(); c_it != all.end(); c_it++) {
	    struct bv_scene_group *cg = *c_it;
	    if (bu_vls_strlen(&upath) > bu_vls_strlen(&cg->s_name)) {
		if (bu_vls_strncmp(&upath, &cg->s_name, bu_vls_strlen(&cg->s_name)) == 0) {
		    // The hard case - upath matches all of sname.  We're
		    // erasing a part of the sname group, so we need to split
		    // it up into new groups.
		    split[cg].insert(std::string(bu_vls_cstr(&upath)));

		    // We may be clearing multiple paths, but only one path per
		    // input path should end up split, so we can stop checking
		    // upath now that it's assigned.
		    break;
		}
	    } else {
		if (bu_vls_strncmp(&upath, &cg->s_name, bu_vls_strlen(&upath)) == 0) {
		    // Easy case.  Drawn path (s_name) is equal to or below
		    // hierarchy of specified upath, so it will be subsumed in
		    // upath - just clear it.  This may happen to multiple
		    // paths, so we don't stop the check if we find a match.
		    clear.insert(cg);
		}
	    }
	}
    }
    bu_vls_free(&upath);

    // If any paths ended up in clear, there's no need to split them even if that
    // would have been the consequence of another path specifier
    for (c_it = clear.begin(); c_it != clear.end(); c_it++) {
	split.erase(*c_it);
    }

    // Do the straight-up removals
    for (c_it = clear.begin(); c_it != clear.end(); c_it++) {
	struct bv_scene_group *cg = *c_it;
	all.erase(cg);
	bv_scene_obj_free(cg, free_scene_obj);
    }

    // Now, the splits.

    // First step - remove the solid children corresponding to the removed
    // paths so cg->children contains only the set of solids to be drawn (it
    // will at that point be an invalid scene group but will be a suitable
    // input for the next stage.)
    for (g_it = split.begin(); g_it != split.end(); g_it++) {
	struct bv_scene_group *cg = &(*g_it->first);
	std::set<struct bv_scene_obj *> sclear;
	std::set<struct bv_scene_obj *>::iterator s_it;
	std::set<std::string>::iterator st_it;
	for (st_it = g_it->second.begin(); st_it != g_it->second.end(); st_it++) {
	    bu_vls_sprintf(&upath, "%s", st_it->c_str());
	    if (bu_vls_cstr(&upath)[0] != '/') {
		bu_vls_prepend(&upath, "/");
	    }
	    // Select the scene objects that contain upath in their hierarchy (i.e. s_name
	    // is a full subset of upath).  These objects are removed.
	    if (bu_vls_strncmp(&upath, &cg->s_name, bu_vls_strlen(&cg->s_name)) == 0) {
		for (size_t j = 0; j < BU_PTBL_LEN(&cg->children); j++) {
		    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(&cg->children, j);
		    // For the solids in cg, any that are below upath in the hierarchy
		    // (that is, upath is a full subset of s_name) are being erased.
		    if (bu_vls_strncmp(&upath, &s->s_name, bu_vls_strlen(&upath)) == 0) {
			sclear.insert(s);
		    }
		    continue;
		}
	    }
	}
	for (s_it = sclear.begin(); s_it != sclear.end(); s_it++) {
	    struct bv_scene_obj *s = *s_it;
	    bu_ptbl_rm(&cg->children, (long *)s);
	    bv_scene_obj_free(s, free_scene_obj);
	}
    }

    // Now, generate the new scene groups and assign the still-active solids to them.
    for (g_it = split.begin(); g_it != split.end(); g_it++) {
	struct bv_scene_group *cg = g_it->first;
	std::set<std::string> &spaths = g_it->second;
	all.erase(cg);
	new_scene_grps(&all, dbip, cg, spaths, free_scene_obj);
    }

    // Repopulate the sg tbl with the final results
    for (c_it = all.begin(); c_it != all.end(); c_it++) {
	struct bv_scene_group *ng = *c_it;
	bu_ptbl_ins(sg, (long *)ng);
    }

    // Sort
    bu_sort(BU_PTBL_BASEADDR(sg), BU_PTBL_LEN(sg), sizeof(struct bv_scene_group *), alphanum_cmp, NULL);

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

