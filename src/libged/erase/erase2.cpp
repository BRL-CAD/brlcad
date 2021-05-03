/*                         E R A S E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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

#include "ged/view/state.h"

#include "../ged_private.h"

// Need to process deepest to shallowest, so we don't get bit by starting at a
// high level path - we need to check the deepest parent, and build up from the
// bottom.  Below sorting criteria need to be defined to sort such that we
// return the deepest paths as the begin() of the set.
struct dfp_cmp {
    bool operator() (struct db_full_path *fp, struct db_full_path *o) const {
	// First, check length
	if (fp->fp_len != o->fp_len) {
	    return (fp->fp_len > o->fp_len);
	}

	// If length is the same, check the dp contents
	for (size_t i = 0; i < fp->fp_len; i++) {
	    if (!BU_STR_EQUAL(fp->fp_names[i]->d_namep, o->fp_names[i]->d_namep)) {
		return (bu_strcmp(fp->fp_names[i]->d_namep, o->fp_names[i]->d_namep) > 0);
	    }
	}
	return (bu_strcmp(fp->fp_names[fp->fp_len-1]->d_namep, o->fp_names[fp->fp_len-1]->d_namep) > 0);
    }
};

void
new_scene_objs(struct bview *v, struct db_i *dbip, struct bu_ptbl *solids)
{
    struct bu_vls pvls = BU_VLS_INIT_ZERO;
    std::set<struct db_full_path *, dfp_cmp> s1, s2;
    std::set<struct db_full_path *, dfp_cmp> *s, *snext;
    s = &s1;
    snext = &s2;

    // Seed the sets with what we know to be drawn.  At this stage,
    // we don't yet know of anything that is not drawn.  We put the
    // known drawn objects into the drawn set, and queue their parents
    // for checking.
    std::set<std::string> not_drawn;
    std::set<std::string> drawn;
    std::set<struct db_full_path *> finalized;
    for (size_t i = 0; i < BU_PTBL_LEN(solids); i++) {
	struct bview_scene_obj *sobj = (struct bview_scene_obj *)BU_PTBL_GET(solids, i);
	struct draw_update_data_t *ud = (struct draw_update_data_t *)sobj->s_i_data;
	drawn.insert(std::string(bu_vls_cstr(&sobj->s_name)));

	// The parent needs to be characterized, if there is one.
	if (ud->fp.fp_len == 1) {
	    // If there isn't anything above the current dp (such as a
	    // solid with no hierarchy is drawn) it's a finalized object
	    struct db_full_path *fpp;
	    BU_GET(fpp, struct db_full_path);
	    db_full_path_init(fpp);
	    db_dup_full_path(fpp, &ud->fp);
	    finalized.insert(fpp);
	} else {
	    // Have path depth - get the parent and queue it up
	    struct db_full_path *fp;
	    BU_GET(fp, struct db_full_path);
	    db_full_path_init(fp);
	    db_dup_full_path(fp, &ud->fp);
	    DB_FULL_PATH_POP(fp);
	    if (s->find(fp) == s->end()) {
		s->insert(fp);
	    } else {
		db_free_full_path(fp);
		BU_PUT(fp, struct db_full_path);
	    }
	}
    }

    // Main characterization loop
    size_t longest = (s->size()) ? (*s->begin())->fp_len : 0;
    while (s->size()) {
	struct db_full_path *pp = *s->begin();
	s->erase(s->begin());

	// If we already know it's not drawn, don't process again
	bu_vls_trunc(&pvls, 0);
	db_path_to_vls(&pvls, pp);
	if (not_drawn.find(std::string(bu_vls_cstr(&pvls))) != not_drawn.end()) {
	    db_free_full_path(pp);
	    BU_PUT(pp, struct db_full_path);
	    if (!s->size()) {
		std::set<struct db_full_path *, dfp_cmp> *tmp = s;
		s = snext;
		snext = tmp;
		longest = (s->size()) ? (*s->begin())->fp_len : 0;
	    }
	    continue;
	}

	// If pp is shorter than the longest paths of this cycle, defer it
	// until the next round.  Longer paths need to be fully processed
	// before we can correctly characterize shorter ones, and it may happen
	// that a shorter path queues a parent that's also a parent in a longer
	// path path before we're ready for it.
	if (pp->fp_len < longest) {
	    snext->insert(pp);
	    if (!s->size()) {
		std::set<struct db_full_path *, dfp_cmp> *tmp = s;
		s = snext;
		snext = tmp;
		longest = (s->size()) ? (*s->begin())->fp_len : 0;
	    }
	    continue;
	}

	// Get the list of immediate children from pp:
	struct directory **children = NULL;
	struct rt_db_internal in;
	struct rt_comb_internal *comb;
	if (rt_db_get_internal(&in, DB_FULL_PATH_CUR_DIR(pp), dbip, NULL, &rt_uniresource) < 0) {
	    not_drawn.insert(std::string(bu_vls_cstr(&pvls)));
	    db_free_full_path(pp);
	    BU_PUT(pp, struct db_full_path);
	    if (!s->size()) {
		std::set<struct db_full_path *, dfp_cmp> *tmp = s;
		s = snext;
		snext = tmp;
		longest = (s->size()) ? (*s->begin())->fp_len : 0;
	    }
	    continue;
	}
	comb = (struct rt_comb_internal *)in.idb_ptr;
	db_comb_children(dbip, comb, &children, NULL, NULL);

	int i = 0;
	int not_found = 0;
	struct directory *dp =  children[0];
	while (dp != RT_DIR_NULL) {
	    db_add_node_to_full_path(pp, dp);
	    bu_vls_trunc(&pvls, 0);
	    db_path_to_vls(&pvls, pp);
	    if (drawn.find(std::string(bu_vls_cstr(&pvls))) == drawn.end()) {
		// This path is not drawn.  That means the parent comb is also
		// not (fully) drawn.
		not_found = 1;
	    }
	    i++;
	    dp = children[i];
	    DB_FULL_PATH_POP(pp);
	}

	// If everything is good, this is a drawn path.  Flag it and insert its
	// parent into the next queue.
	if (!not_found) {
	    bu_vls_trunc(&pvls, 0);
	    db_path_to_vls(&pvls, pp);
	    drawn.insert(std::string(bu_vls_cstr(&pvls)));
	    if (pp->fp_len == 1) {
		// Fully drawn, no parent - finalize
		struct db_full_path *fpp;
		BU_GET(fpp, struct db_full_path);
		db_full_path_init(fpp);
		db_dup_full_path(fpp, pp);
		finalized.insert(fpp);
	    } else {
		DB_FULL_PATH_POP(pp);
		bu_vls_trunc(&pvls, 0);
		db_path_to_vls(&pvls, pp);
		if (pp->fp_len && snext->find(pp) == snext->end()) {
		    struct db_full_path *npp;
		    BU_GET(npp, struct db_full_path);
		    db_full_path_init(npp);
		    db_dup_full_path(npp, pp);
		    snext->insert(npp);
		}
	    }
	    db_free_full_path(pp);
	    BU_PUT(pp, struct db_full_path);
	} else {
	    // If we had a not_found path, we need to finalize any child paths
	    // that are drawn - that is the terminating condition for walking
	    // back up paths.
	    i = 0;
	    dp = children[0];
	    bu_vls_trunc(&pvls, 0);
	    db_path_to_vls(&pvls, pp);

	    while (dp != RT_DIR_NULL) {
		db_add_node_to_full_path(pp, dp);
		bu_vls_trunc(&pvls, 0);
		db_path_to_vls(&pvls, pp);
		if (drawn.find(std::string(bu_vls_cstr(&pvls))) != drawn.end()) {
		    // This path is drawn - finalize
		    struct db_full_path *fpp;
		    BU_GET(fpp, struct db_full_path);
		    db_full_path_init(fpp);
		    db_dup_full_path(fpp, pp);
		    finalized.insert(fpp);
		}
		DB_FULL_PATH_POP(pp);
		i++;
		dp = children[i];
	    }
	    // Handled the children - the parent classifies as not_drawn
	    bu_vls_trunc(&pvls, 0);
	    db_path_to_vls(&pvls, pp);
	    not_drawn.insert(std::string(bu_vls_cstr(&pvls)));
	    db_free_full_path(pp);
	    BU_PUT(pp, struct db_full_path);
	}

	bu_free(children, "free children struct directory ptr array");


	// If s is not empty after this process, we have more paths from other
	// sources to process, and we continue in a similar fashion.  Otherwise,
	// swap snext in for s to see if we have new parents to check.
	if (!s->size()) {
	    std::set<struct db_full_path *, dfp_cmp> *tmp = s;
	    s = snext;
	    snext = tmp;
	    longest = (s->size()) ? (*s->begin())->fp_len : 0;
	}
    }

    // Now we place the solids in new containers.
    std::set<struct bview_scene_obj *> sset;
    std::set<struct bview_scene_obj *>::iterator ss_it;
    for (size_t i = 0; i < BU_PTBL_LEN(solids); i++) {
	struct bview_scene_obj *sobj = (struct bview_scene_obj *)BU_PTBL_GET(solids, i);
	sset.insert(sobj);
    }
    while (finalized.size()) {
	struct db_full_path *fp = *finalized.begin();
	finalized.erase(finalized.begin());

	// Make the new group container
	struct bview_scene_group *g;
	BU_GET(g, struct bview_scene_group);
	BU_LIST_INIT(&(g->g.s_vlist));
	BU_PTBL_INIT(&g->g.children);

	bu_vls_trunc(&pvls, 0);
	db_path_to_vls(&pvls, fp);
	BU_VLS_INIT(&g->g.s_name);
	bu_vls_sprintf(&g->g.s_name, "%s", bu_vls_cstr(&pvls));
	BU_VLS_INIT(&g->g.s_uuid);
	bu_vls_sprintf(&g->g.s_uuid, "%s", bu_vls_cstr(&pvls));

	bu_ptbl_ins(v->gv_db_grps, (long *)g);

	// Populate.  Each solid will go into one container
	std::set<struct bview_scene_obj *> used;
	for (ss_it = sset.begin(); ss_it != sset.end(); ss_it++) {
	    struct bview_scene_obj *sobj = *ss_it;
	    struct draw_update_data_t *ud = (struct draw_update_data_t *)sobj->s_i_data;
	    if (db_full_path_match_top(fp, &ud->fp)) {
		bu_ptbl_ins(&g->g.children, (long *)sobj);
		used.insert(sobj);
	    }
	}
	for (ss_it = used.begin(); ss_it != used.end(); ss_it++) {
	    sset.erase(*ss_it);
	}

	db_free_full_path(fp);
	BU_PUT(fp, struct db_full_path);
    }
}



/*
 * Erase objects from the display.
 *
 */
extern "C" int
ged_erase2_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "[object(s)]";
    const char *cmdName = argv[0];
    struct bview *v = gedp->ged_gvp;
    struct db_i *dbip = gedp->ged_wdbp->dbip;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Check that we have a view */
    if (!v) {
	bu_vls_printf(gedp->ged_result_str, "No current view defined in GED, nothing to erase from");
	return GED_ERROR;
    }

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmdName, usage);
	return GED_HELP;
    }

    /* skip past cmd */
    argc--; argv++;

    /* Validate that the supplied args are current, valid paths.  If not, bail */
    std::set<struct db_full_path *> fps;
    std::set<struct db_full_path *>::iterator f_it;
    for (size_t i = 0; i < (size_t)argc; ++i) {
	struct db_full_path *fp;
	BU_GET(fp, struct db_full_path);
	db_full_path_init(fp);
	int ret = db_string_to_path(fp, dbip, argv[i]);
	if (ret < 0) {
	    // Invalid path
	    db_free_full_path(fp);
	    bu_vls_printf(gedp->ged_result_str, "Invalid path: %s\n", argv[i]);
	}
	fps.insert(fp);
    }
    if (fps.size() != (size_t)argc) {
	for (f_it = fps.begin(); f_it != fps.end(); f_it++) {
	    struct db_full_path *fp = *f_it;
	    db_free_full_path(fp);
	    BU_PUT(fp, struct db_full_path);
	}
	return GED_ERROR;
    }

    // Check the supplied paths against the drawn paths.  If they are equal or
    // if the supplied path contains a drawn path, then we just completely
    // eliminate the group.  If the supplied path is a subset of a drawn path,
    // we're removing only part of the group and have to split the drawn path.
    struct bu_ptbl *sg = gedp->ged_gvp->gv_db_grps;
    std::set<struct bview_scene_group *> clear;
    std::set<struct bview_scene_group *> split;
    std::set<struct bview_scene_group *>::iterator g_it;
    for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
	struct bview_scene_group *cg = (struct bview_scene_group *)BU_PTBL_GET(sg, i);
	struct db_full_path gfp;
	db_full_path_init(&gfp);
	int ret = db_string_to_path(&gfp, dbip, bu_vls_cstr(&cg->g.s_name));
	if (ret < 0) {
	    // If path generation fails, just eliminate it as invalid
	    clear.insert(cg);
	    db_free_full_path(&gfp);
	    continue;
	}
	for (f_it = fps.begin(); f_it != fps.end(); f_it++) {
	    struct db_full_path *fp = *f_it;;
	    if (db_full_path_match_top(fp, &gfp)) {
		// Easy case.  Drawn path is equal to or below hierarchy of
		// specified path - just clear it
		clear.insert(cg);
		db_free_full_path(&gfp);
		break;
	    }
	    if (db_full_path_match_top(&gfp, fp)) {
		// The hard case - we're erasing only part of this group, so
		// we need to split it up into new groups.
		split.insert(cg);
		db_free_full_path(&gfp);
		break;
	    }
	}
    }

    // If any paths ended up in clear, there's no need to split them even if that
    // would have been the consequence of another path specifier
    for (g_it = clear.begin(); g_it != clear.end(); g_it++) {
	split.erase(*g_it);
    }

    // Do the straight-up removals
    for (g_it = clear.begin(); g_it != clear.end(); g_it++) {
	struct bview_scene_group *cg = *g_it;
	bu_ptbl_rm(gedp->ged_gvp->gv_db_grps, (long *)cg);
	bview_scene_obj_free(&cg->g);
	BU_PUT(cg, struct bview_scene_group);
    }

    // Now, the splits.  Each split group will be replaced with one or more new
    // groups with a subset of the parent group's objects.  Consequently, we
    // don't free the group's children - just the group container itself.
    //
    // First step - remove the solid children corresponding to the removed
    // paths so g->g.children matches the set of solids to be drawn (it will at
    // that point be an invalid scene group but will be a suitable input for
    // the next stage.)
    for (g_it = split.begin(); g_it != split.end(); g_it++) {
	struct bview_scene_group *cg = *g_it;
    	struct db_full_path gfp;
	db_full_path_init(&gfp);
	db_string_to_path(&gfp, dbip, bu_vls_cstr(&cg->g.s_name));
	std::set<struct bview_scene_obj *> sclear;
	std::set<struct bview_scene_obj *>::iterator s_it;
	for (f_it = fps.begin(); f_it != fps.end(); f_it++) {
	    struct db_full_path *fp = *f_it;
	    if (db_full_path_match_top(&gfp, fp)) {
		for (size_t i = 0; i < BU_PTBL_LEN(&cg->g.children); i++) {
		    struct bview_scene_obj *s = (struct bview_scene_obj *)BU_PTBL_GET(&cg->g.children, i);
		    struct db_full_path sfp;
		    db_full_path_init(&sfp);
		    int ret = db_string_to_path(&sfp, dbip, bu_vls_cstr(&s->s_name));
		    if (ret < 0) {
			// If path generation fails, just eliminate it as invalid
			sclear.insert(s);
			db_free_full_path(&sfp);
			continue;
		    }
		    if (db_full_path_match_top(fp, &sfp)) {
			sclear.insert(s);
		    }
		    db_free_full_path(&sfp);
		    continue;
		}
	    }
	}
	for (s_it = sclear.begin(); s_it != sclear.end(); s_it++) {
	    struct bview_scene_obj *s = *s_it;
	    bu_ptbl_rm(&cg->g.children, (long *)s);
	    bview_scene_obj_free(s);
	    BU_PUT(s, struct bview_scene_obj);
	}
    }

    // Now, generate the new scene groups based on the solid objects still active.
   for (g_it = split.begin(); g_it != split.end(); g_it++) {
	struct bview_scene_group *cg = *g_it;
	bu_ptbl_rm(gedp->ged_gvp->gv_db_grps, (long *)cg);
	new_scene_objs(gedp->ged_gvp, dbip, &cg->g.children);
	BU_PUT(cg, struct bview_scene_group);
   }

   return GED_OK;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
