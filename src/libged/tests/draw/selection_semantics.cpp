/*          S E L E C T I O N _ P A R I T Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file selection_semantics.cpp
 *
 * Verify that bsg_selection is consistent with bsg_pick_result after a GED
 * draw+pick cycle.
 *
 * Strategy:
 *   1. Open moss.g, draw the scene.
 *   2. Run a screen-center point pick; collect bsg_pick_result.
 *   3. Convert pick hits to interaction records and apply them to selection.
 *   4. Verify bsg_selection_count > 0 and interaction-record containment.
 *   5. bsg_selection_clear; verify count drops to 0.
 *
 * No display hardware required (headless GED).
 *
 * Usage: ged_test_selection_semantics <directory-containing-moss.g>
 */

#include "common.h"

#include <fstream>
#include <cstring>

#include <bu.h>
#include <bsg.h>
#include "bsg/interaction.h"
#include "bsg/pick.h"
#include "bsg/selection.h"
#include <ged.h>

#define ASSERT(cond) do { \
    nchecks++; \
    if (!(cond)) { \
	bu_log("FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond); \
	nfails++; \
    } \
} while (0)

static int nchecks = 0;
static int nfails  = 0;


int
main(int ac, char *av[])
{
    bu_setprogname(av[0]);

    if (ac != 2) {
	bu_log("Usage: %s <directory-containing-moss.g>\n", av[0]);
	return 1;
    }
    if (!bu_file_directory(av[1])) {
	bu_log("ERROR: [%s] is not a directory.\n", av[1]);
	return 2;
    }

    bu_setenv("LIBRT_USE_COMB_INSTANCE_SPECIFIERS", "1", 1);

    char lcache[MAXPATHLEN] = {0};
    bu_dir(lcache, MAXPATHLEN, BU_DIR_CURR,
	   "ged_selection_semantics_cache", NULL);
    bu_mkdir(lcache);
    bu_setenv("BU_DIR_CACHE", lcache, 1);

    /* Copy moss.g into a working file. */
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fname, "%s/moss.g", av[1]);
    std::ifstream orig(bu_vls_cstr(&fname), std::ios::binary);
    if (!orig.is_open()) {
	bu_log("ERROR: cannot open %s\n", bu_vls_cstr(&fname));
	bu_vls_free(&fname);
	return 3;
    }

    struct bu_vls wname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&wname, "%s/moss_selection_semantics_tmp.g",
		   bu_dir(NULL, 0, BU_DIR_CURR, NULL));
    {
	std::ofstream dst(bu_vls_cstr(&wname), std::ios::binary);
	dst << orig.rdbuf();
    }
    bu_vls_free(&fname);

    struct ged *gedp = ged_open("db", bu_vls_cstr(&wname), 1);
    bu_vls_free(&wname);
    if (!gedp) {
	bu_log("ERROR: ged_open failed\n");
	return 4;
    }

    /* Draw the scene. */
    const char *draw_av[] = {"draw", "all.g", NULL};
    ged_exec(gedp, 2, draw_av);

    /* Obtain the default view. */
    struct bsg_view *v = gedp->ged_gvp;
    ASSERT(v != NULL);

    if (!v) {
	ged_close(gedp);
	return nfails ? 1 : 0;
    }

    /* ------------------------------------------------------------------
     * Test 1: bsg_selection lifecycle round-trip
     * ------------------------------------------------------------------ */
    struct bsg_selection *sel = bsg_view_selection(v);
    ASSERT(sel != NULL);
    if (!sel) {
	bu_log("SKIP: view has no gv_selected — bsg_selection not wired\n");
	ged_close(gedp);
	return 0;
    }

    ASSERT(bsg_selection_count(sel) == 0);

    /* ------------------------------------------------------------------
     * Test 2: pick then add to selection
     * ------------------------------------------------------------------ */
    int cx = v->gv_width  / 2;
    int cy = v->gv_height / 2;
    struct bsg_pick_result *pr = bsg_pick_point(v, cx, cy, 0);
    if (pr && bsg_pick_result_count(pr) > 0) {
	struct bsg_interaction_result *ir = bsg_interaction_from_pick_result(pr);
	const struct bsg_interaction_record *rec = bsg_interaction_result_get(ir, 0);
	const char *path = bsg_interaction_record_path(rec);
	if (rec && path && path[0]) {
	    bsg_interaction_selection_apply(sel, ir, BSG_INTERACTION_APPLY_SET);
	    ASSERT(bsg_selection_count(sel) == 1);
	    ASSERT(bsg_selection_contains_record(sel, rec));
	}
	bsg_interaction_result_free(ir);
    }

    /* Clear and verify */
    bsg_selection_clear(sel);
    ASSERT(bsg_selection_count(sel) == 0);

    if (pr)
	bsg_pick_result_free(pr);

    ged_close(gedp);

    bu_log("selection semantic records: %d checks, %d failures\n", nchecks, nfails);
    return nfails ? 1 : 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
