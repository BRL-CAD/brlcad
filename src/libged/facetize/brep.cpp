/*                         B R E P . C P P
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
/** @file libged/facetize/brep.cpp
 *
 * The facetize command's brep method (experimental).
 *
 */

#include "common.h"

#include <vector>
#include <set>

#include "bu/app.h"
#include "brep.h"
#include "raytrace.h"
#include "../ged_private.h"
#include "./ged_facetize.h"

int
_nonovlp_brep_facetize(struct _ged_facetize_state *s, int argc, const char **argv)
{
    int newobj_cnt;
    struct directory **dpa = NULL;

    RT_CHECK_DBI(s->gedp->dbip);

    if (argc <= 0) return BRLCAD_ERROR;

    dpa = (struct directory **)bu_calloc(argc, sizeof(struct directory *), "dp array");
    newobj_cnt = _ged_sort_existing_objs(s->gedp, argc, argv, dpa);
    if (_ged_validate_objs_list(s, argc, argv, newobj_cnt) == BRLCAD_ERROR) {
	bu_free(dpa, "dp array");
	return BRLCAD_ERROR;
    }

    /* If anything specified has subtractions or intersections, we can't facetize it with
     * this logic - that would require all-up Boolean evaluation processing. */
    const char *non_union = "-bool + -or -bool -";
    if (db_search(NULL, DB_SEARCH_QUIET, non_union, newobj_cnt, dpa, s->gedp->dbip, NULL) > 0) {
	bu_free(dpa, "dp array");
	bu_vls_printf(s->gedp->ged_result_str, "Found intersection or subtraction objects in specified inputs - currently unsupported. Aborting.\n");
	return BRLCAD_ERROR;
    }

    /* If anything other than combs or breps exists in the specified inputs, we can't
     * process with this logic - requires a preliminary brep conversion. */
    const char *obj_types = "! -type c -and ! -type brep";
    if (db_search(NULL, DB_SEARCH_QUIET, obj_types, newobj_cnt, dpa, s->gedp->dbip, NULL) > 0) {
	bu_free(dpa, "dp array");
	bu_vls_printf(s->gedp->ged_result_str, "Found objects in specified inputs which are not of type comb or brep- currently unsupported. Aborting.\n");
	return BRLCAD_ERROR;
    }

    /* OK, we have work to do. Set up a working copy of the .g file. */
    //char kfname[MAXPATHLEN];
    if (_ged_facetize_working_file_setup(s, NULL) != BRLCAD_OK) {
	bu_free(dpa, "dp array");
	return BRLCAD_ERROR;
    }

    /* We need all of the inputs for this method to be xpushed - do the xpush
     * operations in the working copy of the .g file. */
    struct ged *wgedp = ged_open("db", bu_vls_cstr(s->wfile), 1);
    if (!wgedp) {
	bu_free(dpa, "dp array");
	return BRLCAD_ERROR;
    }
    for (int i = 0; i < newobj_cnt;  i++) {
	int xac = 3;
	const char *xav[3] = {NULL};
	xav[0] = "xpush";
	xav[1] = dpa[i]->d_namep;
	ged_exec(wgedp, xac, (const char **)xav);
    }

    /* Used the libged tolerances */
    struct rt_wdb *wdbp = wdb_dbopen(wgedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    s->tol = &(wdbp->wdb_ttol);
    struct bg_tess_tol cdttol;
    cdttol.abs = s->tol->abs;
    cdttol.rel = s->tol->rel;
    cdttol.norm = s->tol->norm;

    /* Find breps */
    const char *active_breps = "-type brep";
    struct bu_ptbl *br;
    BU_ALLOC(br, struct bu_ptbl);
    if (db_search(br, DB_SEARCH_RETURN_UNIQ_DP, active_breps, newobj_cnt, dpa, wgedp->dbip, NULL) < 0) {
	bu_free(dpa, "dp array");
	bu_free(br, "brep results");
	return BRLCAD_ERROR;
    }
    if (!BU_PTBL_LEN(br)) {
	/* No active breps (unlikely but technically possible), nothing to do */
	bu_vls_printf(s->gedp->ged_result_str, "No brep objects present in specified inputs - nothing to convert.\n");
	bu_free(dpa, "dp array");
	bu_free(br, "brep results");
	return BRLCAD_OK;
    }

    bu_free(dpa, "dpa array");

    std::set<struct directory *> brep_objs;
    for (int i = BU_PTBL_LEN(br) - 1; i >= 0; i--) {
	struct directory *cobj = (struct directory *)BU_PTBL_GET(br, i);
	brep_objs.insert(cobj);
    }
    bu_free(br, "brep results");

    /* Now, actually trigger the facetize logic. */
    std::vector<ON_Brep_CDT_State *> ss_cdt;
    std::set<struct directory *>::iterator d_it;
    for (d_it = brep_objs.begin(); d_it != brep_objs.end(); ++d_it) {
	struct rt_db_internal intern;
	struct rt_brep_internal* bi;
	GED_DB_GET_INTERNAL(wgedp, &intern, *d_it, bn_mat_identity, &rt_uniresource, BRLCAD_ERROR);
	RT_CK_DB_INTERNAL(&intern);
	bi = (struct rt_brep_internal*)intern.idb_ptr;
	if (!RT_BREP_TEST_MAGIC(bi)) {
	    bu_vls_printf(s->gedp->ged_result_str, "Error: %s is not a brep solid", (*d_it)->d_namep);
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
    struct ON_Brep_CDT_State **s_a = (struct ON_Brep_CDT_State **)bu_calloc(ss_cdt.size(), sizeof(struct ON_Brep_CDT_State *), "state array");
    for (size_t i = 0; i < ss_cdt.size(); i++) {
	s_a[i] = ss_cdt[i];
    }

    int resolve_result = ON_Brep_CDT_Ovlp_Resolve(s_a, ss_cdt.size(), s->nonovlp_threshold, s->max_time);
    if (resolve_result < 0) {
	bu_vls_printf(s->gedp->ged_result_str, "Error: RESOLVE fail.\n");
#if 0
	for (size_t i = 0; i < ss_cdt.size(); i++) {
	    ON_Brep_CDT_Destroy(ss_cdt[i]);
	}
	bu_free(s_a, "array of states");
	return BRLCAD_ERROR;
#endif
    }

    if (resolve_result > 0) {
	bu_vls_printf(s->gedp->ged_result_str, "WARNING: Timeout of %d seconds overlap processing reached, but triangles not fully refined to specified threshold.\nGenerating meshes, but larger overlaps will be present.\n", s->max_time);
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

	if (wdb_export(wdbp, ON_Brep_CDT_ObjName(ss_cdt[i]), (void *)bot, ID_BOT, 1.0)) {
	    bu_vls_printf(s->gedp->ged_result_str, "Error exporting object %s.", ON_Brep_CDT_ObjName(ss_cdt[i]));
	    for (size_t j = 0; j < ss_cdt.size(); j++) {
		ON_Brep_CDT_Destroy(ss_cdt[j]);
	    }
	    return BRLCAD_ERROR;
	}
    }

    /* Done changing stuff in working database. */
    ged_close(wgedp);

    for (size_t i = 0; i < ss_cdt.size(); i++) {
	ON_Brep_CDT_Destroy(ss_cdt[i]);
    }

    /* Keep out just what we asked for into a .g file */
    struct bu_vls kwfile = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&kwfile, "%s_keep", bu_vls_cstr(s->wfile));
    const char **av = (const char **)bu_calloc(argc+10, sizeof(const char *), "av");
    av[0] = "keep";
    av[1] = bu_vls_cstr(&kwfile);
    for (int i = 0; i < argc; i++) {
	av[i+2] = argv[i];
    }
    av[argc+2] = NULL;
    ged_exec(wgedp, argc+2, av);

    /* Merge working geometry into original file */
    av[0] = "dbconcat";
    av[1] = "-L";
    av[2] = bu_vls_cstr(&kwfile);
    av[3] = "brep_facetize_"; // TODO - customize
    av[4] = NULL;
    ged_exec(s->gedp, 4, av);
    bu_free(av, "av");
    bu_vls_free(&kwfile);

    /* Clean up */
    bu_dirclear(s->wdir);

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

