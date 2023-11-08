/*                         B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2023 United States Government as represented by
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
/** @file libged/facetize/brep.cpp
 *
 * The facetize command's brep method (experimental).
 *
 */

#include "common.h"

#include <vector>
#include <set>

#include "brep.h"
#include "raytrace.h"
#include "../ged_private.h"
#include "./ged_facetize.h"

int
_nonovlp_brep_facetize(struct _ged_facetize_state *s, struct ged *gedp, int argc, const char **argv)
{
    char *newname = NULL;
    int newobj_cnt = 0;
    struct directory **dpa = NULL;
    struct db_i *dbip = gedp->dbip;
    struct bu_ptbl *ac = NULL;
    struct bu_ptbl *br = NULL;
    std::vector<ON_Brep_CDT_State *> ss_cdt;
    struct ON_Brep_CDT_State **s_a = NULL;

    /* Used the libged tolerances */
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    s->tol = &(wdbp->wdb_ttol);
    struct bg_tess_tol cdttol;
    cdttol.abs = s->tol->abs;
    cdttol.rel = s->tol->rel;
    cdttol.norm = s->tol->norm;

    if (!argc) return BRLCAD_ERROR;

    dpa = (struct directory **)bu_calloc(argc, sizeof(struct directory *), "dp array");
    newobj_cnt = _ged_sort_existing_objs(gedp, argc, argv, dpa);
    if (_ged_validate_objs_list(s, argc, argv, newobj_cnt) == BRLCAD_ERROR) {
	bu_free(dpa, "free dpa");
	return BRLCAD_ERROR;
    }

    /* If anything specified has subtractions or intersections, we can't facetize it with
     * this logic - that would require all-up Boolean evaluation processing. */
    const char *non_union = "-bool + -or -bool -";
    if (db_search(NULL, DB_SEARCH_QUIET, non_union, newobj_cnt, dpa, dbip, NULL) > 0) {
	bu_vls_printf(gedp->ged_result_str, "Found intersection or subtraction objects in specified inputs - currently unsupported. Aborting.\n");
	return BRLCAD_ERROR;
    }

    /* If anything other than combs or breps exists in the specified inputs, we can't
     * process with this logic - requires a preliminary brep conversion. */
    const char *obj_types = "! -type c -and ! -type brep";
    if (db_search(NULL, DB_SEARCH_QUIET, obj_types, newobj_cnt, dpa, dbip, NULL) > 0) {
	bu_vls_printf(gedp->ged_result_str, "Found objects in specified inputs which are not of type comb or brep- currently unsupported. Aborting.\n");
	return BRLCAD_ERROR;
    }

    /* Find breps (need full paths to do uniqueness checking )*/
    const char *active_breps = "-type brep";
    BU_ALLOC(br, struct bu_ptbl);
    if (db_search(br, DB_SEARCH_TREE, active_breps, newobj_cnt, dpa, dbip, NULL) < 0) {
	bu_free(br, "brep results");
	return BRLCAD_ERROR;
    }
    if (!BU_PTBL_LEN(br)) {
	/* No active breps (unlikely but technically possible), nothing to do */
	bu_vls_printf(gedp->ged_result_str, "No brep objects present in specified inputs - nothing to convert.\n");
	bu_free(br, "brep results");
	return BRLCAD_OK;
    }

    /* Find combs (need full paths to do uniqueness checking) */
    const char *active_combs = "-type c";
    BU_ALLOC(ac, struct bu_ptbl);
    if (db_search(ac, DB_SEARCH_TREE, active_combs, newobj_cnt, dpa, dbip, NULL) < 0) {
	if (s->verbosity) {
	    bu_log("Problem searching for parent combs - aborting.\n");
	}
	bu_free(br, "brep results");
	bu_free(ac, "comb results");
	return BRLCAD_ERROR;
    }

    bu_free(dpa, "dpa array");

    /* When doing a non-overlapping tessellation, non-unique object instances won't work -
     * a tessellation of one instance of an object may be clear of overlaps, but the same
     * tessellation in another location may interfere.  There are various situations where
     * this is may be OK, but if I'm not mistaken real safety requires a spatial check of some
     * sort to ensure breps or combs used in multiple places are isolated enough that their
     * tessellations won't be a potential source of overlaps.
     *
     * For now, just report the potential issue (recommending xpush, which is the best option
     * I know of to try and deal with this currently) and quit.
     */

    std::set<struct directory *> brep_objs;
    bool multi_instance = false;
    for (int i = BU_PTBL_LEN(br) - 1; i >= 0; i--) {
	struct db_full_path *dfptr = (struct db_full_path *)BU_PTBL_GET(br, i);
	struct directory *cobj = DB_FULL_PATH_CUR_DIR(dfptr);
	if (brep_objs.find(cobj) != brep_objs.end()) {
	    bu_vls_printf(gedp->ged_result_str, "Multiple instances of %s observed.\n", cobj->d_namep);
	    multi_instance = true;
	} else {
	    brep_objs.insert(cobj);
	}
    }
    std::set<struct directory *> comb_objs;
    for (int i = BU_PTBL_LEN(ac) - 1; i >= 0; i--) {
	struct db_full_path *dfptr = (struct db_full_path *)BU_PTBL_GET(ac, i);
	struct directory *cobj = DB_FULL_PATH_CUR_DIR(dfptr);
	if (comb_objs.find(cobj) != comb_objs.end()) {
	    bu_vls_printf(gedp->ged_result_str, "Multiple instances of %s observed.\n", cobj->d_namep);
	    multi_instance = true;
	} else {
	    comb_objs.insert(cobj);
	}
    }
    if (multi_instance) {
	bu_vls_printf(gedp->ged_result_str, "Multiple object instances found - suggest using xpush command to create unique objects.\n");
	bu_free(br, "brep results");
	bu_free(ac, "comb results");
	return BRLCAD_ERROR;
    }
    bu_free(br, "brep results");
    bu_free(ac, "comb results");



    newname = (char *)argv[argc-1];
    argc--;

    /* Use the new name for the root */
    bu_vls_sprintf(s->froot, "%s", newname);

    /* Set up all the names we will need */
    std::set<struct directory *>::iterator d_it;
    for (d_it = brep_objs.begin(); d_it != brep_objs.end(); d_it++) {
	_ged_facetize_mkname(s, (*d_it)->d_namep, SOLID_OBJ_NAME);
    }
    for (d_it = comb_objs.begin(); d_it != comb_objs.end(); d_it++) {
	_ged_facetize_mkname(s, (*d_it)->d_namep, COMB_OBJ_NAME);
    }

    /* First, add the new toplevel comb to hold all the new geometry */
    const char **ntop = (const char **)bu_calloc(argc, sizeof(const char *), "new top level names");
    for (int i = 0; i < argc; i++) {
	ntop[i] = bu_avs_get(s->c_map, argv[i]);
	if (!ntop[i]) {
	    ntop[i] = bu_avs_get(s->s_map, argv[i]);
	}
    }
    if (!s->quiet) {
	bu_log("Creating new top level assembly object %s...\n", newname);
    }
    _ged_combadd2(gedp, newname, argc, ntop, 0, DB_OP_UNION, 0, 0, NULL, 0);
    bu_free(ntop, "new top level names");

    /* For the combs, make new versions with the suffixed names */
    if (!s->quiet) {
	bu_log("Initializing copies of combinations...\n");
    }
    for (d_it = comb_objs.begin(); d_it != comb_objs.end(); d_it++) {
	struct directory *n = *d_it;

	if (_ged_facetize_cpcomb(s, n->d_namep) != BRLCAD_OK) {
	    if (s->verbosity) {
		bu_log("Failed to creating comb %s for %s \n", bu_avs_get(s->c_map, n->d_namep), n->d_namep);
	    }
	    continue;
	}

	/* Add the members from the map with the settings from the original
	 * comb */
	if (_ged_facetize_add_children(s, n) != BRLCAD_OK) {
	    if (!s->quiet) {
		bu_log("Error: duplication of assembly %s failed!\n", n->d_namep);
	    }
	    continue;
	}
    }

    /* Now, actually trigger the facetize logic. */
    for (d_it = brep_objs.begin(); d_it != brep_objs.end(); d_it++) {
	struct rt_db_internal intern;
	struct rt_brep_internal* bi;
	GED_DB_GET_INTERNAL(gedp, &intern, *d_it, bn_mat_identity, &rt_uniresource, BRLCAD_ERROR);
	RT_CK_DB_INTERNAL(&intern);
	bi = (struct rt_brep_internal*)intern.idb_ptr;
	if (!RT_BREP_TEST_MAGIC(bi)) {
	    bu_vls_printf(gedp->ged_result_str, "Error: %s is not a brep solid", (*d_it)->d_namep);
	    for (size_t i = 0; i < ss_cdt.size(); i++) {
		ON_Brep_CDT_Destroy(ss_cdt[i]);
	    }
	    return BRLCAD_ERROR;
	}
	ON_Brep_CDT_State *s_cdt = ON_Brep_CDT_Create((void *)bi->brep, (*d_it)->d_namep);
	ON_Brep_CDT_Tol_Set(s_cdt, &cdttol);
	ss_cdt.push_back(s_cdt);
    }

    for (size_t i = 0; i < ss_cdt.size(); i++) {
	ON_Brep_CDT_Tessellate(ss_cdt[i], 0, NULL);
    }

    // Do comparison/resolution
    s_a = (struct ON_Brep_CDT_State **)bu_calloc(ss_cdt.size(), sizeof(struct ON_Brep_CDT_State *), "state array");
    for (size_t i = 0; i < ss_cdt.size(); i++) {
	s_a[i] = ss_cdt[i];
    }

    int resolve_result = ON_Brep_CDT_Ovlp_Resolve(s_a, ss_cdt.size(), s->nonovlp_threshold, s->max_time);
    if (resolve_result < 0) {
	bu_vls_printf(gedp->ged_result_str, "Error: RESOLVE fail.\n");
#if 0
	for (size_t i = 0; i < ss_cdt.size(); i++) {
	    ON_Brep_CDT_Destroy(ss_cdt[i]);
	}
	bu_free(s_a, "array of states");
	return BRLCAD_ERROR;
#endif
    }

    if (resolve_result > 0) {
	bu_vls_printf(gedp->ged_result_str, "WARNING: Timeout of %d seconds overlap processing reached, but triangles not fully refined to specified threshold.\nGenerating meshes, but larger overlaps will be present.\n", s->max_time);
    }


    bu_free(s_a, "array of states");

    // Make final meshes
    for (size_t i = 0; i < ss_cdt.size(); i++) {
	int fcnt, fncnt, ncnt, vcnt;
	int *faces = NULL;
	fastf_t *vertices = NULL;
	int *face_normals = NULL;
	fastf_t *normals = NULL;

	ON_Brep_CDT_Mesh(&faces, &fcnt, &vertices, &vcnt, &face_normals, &fncnt, &normals, &ncnt, ss_cdt[i], 0, NULL);

	struct rt_bot_internal *bot;
	BU_GET(bot, struct rt_bot_internal);
	bot->magic = RT_BOT_INTERNAL_MAGIC;
	bot->mode = RT_BOT_SOLID;
	bot->orientation = RT_BOT_CCW;
	bot->bot_flags = 0;
	bot->num_vertices = vcnt;
	bot->num_faces = fcnt;
	bot->vertices = vertices;
	bot->faces = faces;
	bot->thickness = NULL;
	bot->face_mode = (struct bu_bitv *)NULL;
	bot->num_normals = ncnt;
	bot->num_face_normals = fncnt;
	bot->normals = normals;
	bot->face_normals = face_normals;

	if (wdb_export(wdbp, bu_avs_get(s->s_map, ON_Brep_CDT_ObjName(ss_cdt[i])), (void *)bot, ID_BOT, 1.0)) {
	    bu_vls_printf(gedp->ged_result_str, "Error exporting object %s.", bu_avs_get(s->s_map, ON_Brep_CDT_ObjName(ss_cdt[i])));
	    for (size_t j = 0; j < ss_cdt.size(); j++) {
		ON_Brep_CDT_Destroy(ss_cdt[j]);
	    }
	    return BRLCAD_ERROR;
	}
    }

    /* Done changing stuff - update nref. */
    db_update_nref(gedp->dbip, &rt_uniresource);

    for (size_t i = 0; i < ss_cdt.size(); i++) {
	ON_Brep_CDT_Destroy(ss_cdt[i]);
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

