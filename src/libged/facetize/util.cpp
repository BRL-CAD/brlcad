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

#include <string.h>

#include <iostream>
#include <fstream>

#include "bu/app.h"
#include "bu/path.h"
#include "bu/ptbl.h"
#include "rt/search.h"
#include "rt/db_instance.h"
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

int
_ged_facetize_working_file_setup(char **wfile, char **wdir, struct db_i *dbip, struct bu_ptbl *leaf_dps, int resume)
{
    if (!dbip || !wfile || !wdir)
	return BRLCAD_ERROR;

    if (!*wfile || !*wdir) {
	(*wfile) = (char *)bu_calloc(MAXPATHLEN, sizeof(char), "wfile");
	(*wdir) = (char *)bu_calloc(MAXPATHLEN, sizeof(char), "wdir");
	/* Figure out the working .g filename */
	struct bu_vls wfilename = BU_VLS_INIT_ZERO;
	struct bu_vls bname = BU_VLS_INIT_ZERO;
	struct bu_vls dname = BU_VLS_INIT_ZERO;
	char rfname[MAXPATHLEN];
	bu_file_realpath(dbip->dbi_filename, rfname);

	// Get the root filename, so we can give the working file a relatable name
	bu_path_component(&bname, rfname, BU_PATH_BASENAME);

	// Hash the path string and construct
	unsigned long long hash_num = bu_data_hash((void *)bu_vls_cstr(&bname), bu_vls_strlen(&bname));
	bu_vls_sprintf(&dname, "facetize_%llu", hash_num);
	bu_vls_sprintf(&wfilename, "facetize_%s", bu_vls_cstr(&bname));
	bu_vls_free(&bname);

	// Have filename, get a location in the cache directory
	bu_dir(*wdir, MAXPATHLEN, BU_DIR_CACHE, bu_vls_cstr(&dname), NULL);
	bu_dir(*wfile, MAXPATHLEN, BU_DIR_CACHE, bu_vls_cstr(&dname), bu_vls_cstr(&wfilename), NULL);
	bu_vls_free(&dname);
	bu_vls_free(&wfilename);
    }

    /* If we're resuming and we already have an appropriately named .g file, we
     * don't overwrite it.  Otherwise we need to prepare the working copy. */
    if (!resume || (resume && !bu_file_exists(*wfile, NULL))) {
	// If we need to clear an old working copy, do it now
	bu_dirclear(*wdir);
	bu_mkdir(*wdir);

	// Populate the working copy with original .g data
	// (TODO - should use the dbip's FILE pointer for this rather than
	// opening it again if we can (see FIO24-C), but that's a private entry
	// in db_i per the header.  Maybe need a function to return a FILE *
	// from a struct db_i?)
	std::ifstream orig_file(dbip->dbi_filename, std::ios::binary);
	std::ofstream work_file(*wfile, std::ios::binary);
	if (!orig_file.is_open() || !work_file.is_open())
	    return BRLCAD_ERROR;
	work_file << orig_file.rdbuf();
	orig_file.close();
	work_file.close();

	// Must also copy any files referenced by the .g into the proper
	// relative position to the working .g copy.
	for (size_t i = 0; i < BU_PTBL_LEN(leaf_dps); i++) {
	    struct directory *ldp = (struct directory *)BU_PTBL_GET(leaf_dps, i);
	    if (ldp->d_major_type != DB5_MAJORTYPE_BRLCAD)
		continue;

	    if (ldp->d_minor_type == ID_DSP) {
		struct rt_db_internal intern;
		if (rt_db_get_internal(&intern, ldp, dbip, NULL, &rt_uniresource) < 0)
		    continue;
		dsp_data_cpy(dbip, (struct rt_dsp_internal *)intern.idb_ptr, *wdir);
		rt_db_free_internal(&intern);
	    }

	    // TODO - There may be other such cases...
	}
    }

    if (!bu_file_exists(*wfile, NULL))
	return BRLCAD_ERROR;

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

